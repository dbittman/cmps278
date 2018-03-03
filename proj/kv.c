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
	({ (typeof(ptr))_ptr_translate(db,type,(uintptr_t)(ptr)); })

#define ptr_canon(db, type, ptr) \
	({ (typeof(ptr))_ptr_canon(db,type,(uintptr_t)(ptr)); })

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

static uint64_t djb2(void *_data, size_t len)
{
	unsigned char *data = _data;
	unsigned long hash = 5381;
	int c;
	while(len--)
		hash = ((hash << 5) + hash) ^ *data++;
	return hash;
}

static uint64_t hash(void *data, size_t len, int blevel)
{
	return djb2(data, len) & ((1 << blevel) - 1);
}

#define __boundinc(i,l) \
	({ (i+1) & ((1 << l) - 1); })

static bool __compare(DB *db, struct bucket *b, DBT *v)
{
	return v->size == b->len && !memcmp(ptr_translate(db, b->ptr), v->data, b->len);
}

static void *__loadin(DB *db, DBT *v)
{
	
}

static void __rehash(DB *db, size_t sz)
{
	
}

#define DELETED (void *)-1
static int __insert(DB *db, DBT *key, DBT *data)
{
	uint64_t h = hash(key->data, key->size, db->hdr->blevel);
	uint64_t i = h;
	do {
		if(db->hdr->buckets[i].ptr == NULL) {
			break;
		} else if(db->hdr->buckets[i].ptr == DELETED) {
			break;
		} else if(__compare(db, &db->hdr->buckets[i], key)) {
			return EEXIST;
		}
		i = __boundinc(i, db->hdr->blevel);
	} while(i != h);
	if(i == h) {
		abort();
	}
	
	void *p = __loadin(db, key);
	db->hdr->buckets[i].ptr = p;
	db->hdr->buckets[i].len = key->size;
	return 0;
}

static int _db_put(DB *db, DB_TXN *txnid, DBT *key, DBT *data, uint32_t flags)
{
	if(flags != 0) {
		return ENOTSUP;
	}
	DEBUG("putting %s(%ld):%s(%ld)\n", key->data, key->size, data->data, data->size);

	if(db->hdr->count * 4 > (1 << db->hdr->blevel) - 1) {
		__rehash(db, db->hdr->blevel + 1);
	}

	return __insert(db, key, data);
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

