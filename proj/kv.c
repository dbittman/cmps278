#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <sys/mman.h>
#include "nvkv.h"

#define PAGE_SIZE 0x1000
#define TYPE_DATA 0
#define TYPE_META 1

static uintptr_t _ptr_translate(DB *db, int type, uintptr_t offset)
{
	return (uintptr_t)db->base + (offset / PAGE_SIZE)*2*PAGE_SIZE + (offset % PAGE_SIZE) + (type == TYPE_DATA ? PAGE_SIZE : 0);
}

static uintptr_t _ptr_canon(DB *db, int type, uintptr_t ptr)
{
	uintptr_t addr = (ptr - (uintptr_t)db->base) - (type == TYPE_DATA ? PAGE_SIZE : 0);
	return addr - (addr / PAGE_SIZE)*(PAGE_SIZE / 2);
}

#define ptr_translate(db, type, ptr) \
	__extension__({ (typeof(ptr))_ptr_translate(db,type,(uintptr_t)(ptr)); })

#define ptr_canon(db, type, ptr) \
	__extension__({ (typeof(ptr))_ptr_canon(db,type,(uintptr_t)(ptr)); })

static int _db_open(DB *db, DB_TXN *txnid, const char *file, 
		const char *database, DBTYPE type, uint32_t flags, int mode)
{
	int ret;
	int ofl = O_RDWR;
	bool created = false;
	if(flags & DB_CREATE) ofl |= O_CREAT;
	if(flags & DB_EXCL)   ofl |= O_EXCL;

	db->fd = open(file, ofl, mode);
	if(db->fd < 0) {
		return errno;
	}
	struct stat st;
	if(fstat(db->fd, &st) != 0) {
		goto err_close;
	}
	if((flags & DB_CREATE) && st.st_size == 0) {
		st.st_size = sizeof(struct dbheader);
		if(ftruncate(db->fd, st.st_size) != 0) {
			goto err_close;
		}
		created = true;
	}
	db->base = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE,
			MAP_SHARED, db->fd, 0);
	if(db->base == MAP_FAILED) {
		goto err_close;
	}

	db->hdr = db->base;
	if(created) {
		db->hdr->type = type;
		db->hdr->flags = 0;
		db->hdr->magic = _NVKV_MAGIC;
	} else if(db->hdr->magic != _NVKV_MAGIC) {
		munmap(db->base, st.st_size);
		errno = EINVAL;
		goto err_close;
	}
	db->size = st.st_size;

	return 0;
err_close:
	ret = errno;
	close(db->fd);
	return ret;
}

__attribute__((const))
static struct bucket *__get_bucket(DB *db, size_t b)
{
	struct bucket *bucket = ((struct bucket *)sizeof(*db->hdr)) + b;
	return ptr_translate(db, TYPE_META, bucket);
}

static bool __compare(DB *db, struct bucket *b, DBT *v)
{
	struct dbitem *item = ptr_translate(db, TYPE_DATA, b->kp);
	return v->size == item->len && !memcmp(item->data, v->data, v->size);
}

#define GOLDEN_RATIO_PRIME_64 0x9e37fffffffc0001UL

static uint64_t hash_gr_64(const void *data, size_t len, int bits)
{
	uint64_t value = GOLDEN_RATIO_PRIME_64;
	for(size_t i=0;i<len / 8;i++) {
		const uint64_t *d = data;
		value ^= d[i] * GOLDEN_RATIO_PRIME_64;
	}
	return value >> (64 - bits);
}

static uint64_t hash_djb2_64(const void *d, size_t len, int bits)
{
	uint64_t hash = 5381;
	const unsigned char *data = (const unsigned char *)d;
	for(size_t i=0;i<len;i++) {
		hash = ((hash << 5) + hash) ^ *data++;
	}
	return hash & ((1 << bits) - 1);
}

#define USED 1
#define SECOND 2

static void __do_insert(DB *db, size_t b, struct dbitem *key, struct dbitem *item, int fl)
{
	struct bucket *bucket = __get_bucket(db, b);
	bucket->kp = key;
	bucket->dp = item;
	bucket->flags = fl;
}

static int __move(DB *db, size_t b)
{
	uint64_t fl;
	size_t partner;
	struct bucket *bucket = __get_bucket(db, b);
	struct bucket tmp = *bucket;
	struct dbitem *key = ptr_translate(db, TYPE_DATA, tmp.kp);
	if(tmp.flags == 2) {
		partner = hash_gr_64(key->data, key->len, db->hdr->blevel);
		fl = 1;
	} else {
		partner = hash_djb2_64(key->data, key->len, db->hdr->blevel);
		fl = 2;
	}
	bucket->kp = 0;
	bucket->flags = 0;
	struct bucket *partner_bucket = __get_bucket(db, partner);
	if(partner_bucket->flags) {
		__move(db, partner);
	}
	if(partner_bucket->flags != 0)
		return 0;
	__do_insert(db, partner, tmp.kp, tmp.dp, fl);
	return 1;
}

static int __insert(DB *db, struct dbitem *__key, struct dbitem *__item)
{
	struct dbitem *key = ptr_translate(db, TYPE_DATA, __key);
	struct dbitem *item = ptr_translate(db, TYPE_DATA, __item);
	
	size_t b1 = hash_gr_64(key->data, key->len, db->hdr->blevel);
	size_t b2 = hash_djb2_64(key->data, key->len, db->hdr->blevel);

	struct bucket *buck1 = __get_bucket(db, b1);
	struct bucket *buck2 = __get_bucket(db, b2);

	if(buck1->flags == 0) {
		__do_insert(db, b1, __key, __item, 1);
	} else if(buck2->flags == 0) {
		__do_insert(db, b2, __key, __item, 2);
	} else if(buck1->flags == 1) {
		if(__move(db, b1) == 0) goto a;
		__do_insert(db, b1, __key, __item, 1);
	} else if(buck2->flags == 1) {
a:
		if(__move(db, b2) == 0) return 0;
		__do_insert(db, b2, __key, __item, 1);
	} else {
		if(__move(db, b1) == 0) return 0;
		__do_insert(db, b1, __key, __item, 1);
	}
	return 1;
}

static struct dbitem *__lookup(DB *db, DBT *search, int lev)
{
	size_t b1 = hash_gr_64(search->data, search->size, lev);
	size_t b2 = hash_djb2_64(search->data, search->size, lev);

	struct bucket *buck1 = __get_bucket(db, b1);
	struct bucket *buck2 = __get_bucket(db, b2);

	if(buck1->flags && __compare(db, buck1, search)) {
		return buck1->dp;
	} else if(buck2->flags && __compare(db, buck1, search)) {
		return buck2->dp;
	}
	return NULL;
}

static void *lookup(DB *db, DBT *search)
{
	void *ret;
	for(unsigned int i=db->hdr->minlevel;i<=db->hdr->blevel;i++) {
		if((ret=__lookup(db, search, i))) return ret;
	}
	return NULL;
}

static bool insert(DB *db, struct dbitem *__key, struct dbitem *__item)
{
	struct dbitem *key = ptr_translate(db, TYPE_DATA, __key);
	DBT search = {.data = key->data, .size = key->len};
	if(lookup(db, &search)) return false;
	db->hdr->count++;
	if(db->hdr->count * 4 >= (1ul << db->hdr->blevel)) {
r:
		db->hdr->blevel++;
	}
	if(__insert(db, __key, __item) == 0) goto r;
	return true;
}
























static void *__loadin(DB *db, DBT *v)
{
	
}

static int _db_put(DB *db, DB_TXN *txnid, DBT *key, DBT *data, uint32_t flags)
{
	if(flags != 0) {
		return ENOTSUP;
	}
	DEBUG("putting %s(%ld):%s(%ld)\n", key->data, key->size, data->data, data->size);
}

static int _db_get(DB *db, DB_TXN *txnid, DBT *key, DBT *data, uint32_t flags)
{
	if(flags != 0) {
		return ENOTSUP;
	}
}

static int _db_del(DB *db, DB_TXN *txnid, DBT *key, uint32_t flags)
{

}

static int _db_close(DB *db, uint32_t flags __unused)
{
	/* TODO: sync? */
	free(db);
	return 0;
}

static int _db_sync(DB *db, uint32_t flags)
{

}

static void _db_err(DB *db, int error, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	fprintf(stderr, ": %s\n", db_strerror(error));
	va_end(va);
}

static void _db_errx(DB *db, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
}

int db_create(DB **dbp, DB_ENV *dbenv, uint32_t flags)
{
	if(dbenv != NULL) return ENOTSUP;

	*dbp = malloc(sizeof(**dbp));
	if(!*dbp) {
		return ENOMEM;
	}
	memset(*dbp, 0, sizeof(**dbp));

	(*dbp)->open = _db_open;
	(*dbp)->put = _db_put;
	(*dbp)->get = _db_get;
	(*dbp)->sync = _db_sync;
	(*dbp)->err = _db_err;
	(*dbp)->errx = _db_errx;
	(*dbp)->del = _db_del;
	(*dbp)->close = _db_close;

	return 0;
}

