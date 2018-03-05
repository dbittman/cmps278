#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdarg.h>
#define _GNU_SOURCE
#include <sys/mman.h>
#include "nvkv.h"
#include <time.h>
#include <sys/time.h>

#define PAGE_SIZE 0x1000
#define TYPE_DATA 1
#define TYPE_META 0
void timespec_diff(struct timespec *start, struct timespec *stop,
                   struct timespec *result)
{
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }
}


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

#define ALIGNON(v,a) \
	__extension__({ (((v)-1) & ~((a)-1)) + (a); })

static void grow_database(DB *db, void *ptr)
{
	uint64_t off = ALIGNON((uintptr_t)ptr - (uintptr_t)db->base, PAGE_SIZE);
	if(off <= db->size) return;
	munmap(db->base, db->size);
	db->size = off;
	if(ftruncate(db->fd, db->size) < 0) {
		abort();
	}
	db->base = mmap(db->base, db->size, PROT_READ | PROT_WRITE, MAP_SHARED, db->fd, 0);
	db->hdr = db->base;
	//fprintf(stderr, ":: grow %p -> %p\n", db->base, (char *)db->base + db->size);
}

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
	db->size = st.st_size;
	if(created) {
		db->hdr->type = type;
		db->hdr->flags = 0;
		db->hdr->magic = _NVKV_MAGIC;
		db->hdr->blevel = 10;
		db->hdr->minlevel = 10;
		uint64_t bmax = ((1ull << db->hdr->blevel) - 1);
		grow_database(db, ptr_translate(db, TYPE_META, (void *)(bmax * sizeof(struct bucket) + PAGE_SIZE)));
	} else if(db->hdr->magic != _NVKV_MAGIC) {
		munmap(db->base, st.st_size);
		errno = EINVAL;
		goto err_close;
	}

	return 0;
err_close:
	ret = errno;
	close(db->fd);
	return ret;
}

__attribute__((const))
static struct bucket *__get_bucket(DB *db, size_t b)
{
	struct bucket *bucket = ((struct bucket *)PAGE_SIZE) + b;
	//fprintf(stderr, ":: %p %ld -> %p\n", bucket, b, ptr_translate(db, TYPE_META, bucket));
	return ptr_translate(db, TYPE_META, bucket);
}

static bool __compare(DB *db, struct bucket *b, DBT *v)
{
	struct dbitem *item = ptr_translate(db, TYPE_DATA, b->kp);
	bool r = v->size == item->len && !memcmp(item->data, v->data, v->size);
	return r;
}

#define GOLDEN_RATIO_PRIME_64 0x9e37fffffffc0001UL

#define get16bits(d) (*((const uint16_t *) (d)))

/* Peter Weinberger's */
int hashpjw(const char *s, size_t len)
{
	const char *p;
	unsigned int h, g;
	h = 0;
	for(p=s; len--; p++){
		h = (h<<4) + *p;
		if (g = h&0xF0000000) {
			h ^= g>>24;
			h ^= g;
		}
	}
	return h;
}

//http://www.azillionmonkeys.com/qed/hash.html
static uint32_t SuperFastHash (const char * data, int len) {
	uint32_t hash = len, tmp;
	int rem;

    if (len <= 0 || data == NULL) return 0;

    rem = len & 3;
    len >>= 2;

    /* Main loop */
    for (;len > 0; len--) {
        hash  += get16bits (data);
        tmp    = (get16bits (data+2) << 11) ^ hash;
        hash   = (hash << 16) ^ tmp;
        data  += 2*sizeof (uint16_t);
        hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
        case 3: hash += get16bits (data);
                hash ^= hash << 16;
                hash ^= ((signed char)data[sizeof (uint16_t)]) << 18;
                hash += hash >> 11;
                break;
        case 2: hash += get16bits (data);
                hash ^= hash << 11;
                hash += hash >> 17;
                break;
        case 1: hash += (signed char)*data;
                hash ^= hash << 10;
                hash += hash >> 1;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    return hash;
}

static uint64_t hash_gr_64(const void *data, size_t len, int bits)
{
	uint64_t value = ~0;

	size_t i=0;
	for(;i<len / 8;i++) {
		const uint64_t *d = data;
		value ^= d[i] * GOLDEN_RATIO_PRIME_64;
	}
	if(i == 0) {
		abort();
		for(;i<len;i++) {
			const uint8_t *d = data;
			value += d[i];
		}
	}
	return (value * GOLDEN_RATIO_PRIME_64) >> (64 - bits);
}

//http://www.cse.yorku.ca/~oz/hash.html
static uint64_t hash_djb2_64(const void *d, size_t len, int bits)
{
	uint64_t hash = 5381;
	const unsigned char *data = (const unsigned char *)d;
	for(size_t i=0;i<len;i++) {
		hash = ((hash << 5) + hash) ^ *data++;
	}
	return (hash ^ (SuperFastHash(d, len))) & ((1 << bits) - 1);
}

#define USED 1
#define SECOND 2

static void __do_insert(DB *db, size_t b, struct dbitem *key, struct dbitem *item, int fl)
{
	struct bucket *bucket = __get_bucket(db, b);
	if(bucket->flags && bucket->kp) {
		fprintf(stderr, "WHAT %ld: %p %lx\n", b, bucket->kp, bucket->flags);
		abort();
	}
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
	if(b == partner) return 0;
	bucket->kp = NULL;
	bucket->flags = 0;
	struct bucket *partner_bucket = __get_bucket(db, partner);
	if(partner_bucket->flags) {
		__move(db, partner);
	}

	if(partner_bucket->flags && bucket->flags) {
		abort();
	}

	if(partner_bucket->flags) {
		__do_insert(db, b, tmp.kp, tmp.dp, fl);
		return 0;
	} else if(bucket->flags) {
		__do_insert(db, partner, tmp.kp, tmp.dp, fl);
		return 0;
	}
	__do_insert(db, partner, tmp.kp, tmp.dp, fl);
	return 1;
}

static int __insert(DB *db, struct dbitem *__key, struct dbitem *__item)
{
	struct dbitem *key = ptr_translate(db, TYPE_DATA, __key);
	size_t b1 = hash_gr_64(key->data, key->len, db->hdr->blevel);
	struct bucket *buck1 = __get_bucket(db, b1);

	if(buck1->flags == 0) {
		__do_insert(db, b1, __key, __item, 1);
		return 1;
	}

	size_t b2 = hash_djb2_64(key->data, key->len, db->hdr->blevel);
	if(b1 == b2) {
		return 0;
	}
	struct bucket *buck2 = __get_bucket(db, b2);

	if(buck2->flags == 0) {
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
	struct bucket *buck1 = __get_bucket(db, b1);

	if(buck1->flags && __compare(db, buck1, search)) {
		return buck1->dp;
	}

	size_t b2 = hash_djb2_64(search->data, search->size, lev);
	struct bucket *buck2 = __get_bucket(db, b2);
	
	if(buck2->flags && __compare(db, buck2, search)) {
		return buck2->dp;
	}

	return NULL;
}

static void *lookup(DB *db, DBT *search)
{
	void *ret;
	for(unsigned int i=db->hdr->blevel;i>=db->hdr->minlevel;i--) {
		if((ret = __lookup(db, search, i))) return ret;
	}
	return NULL;
}

static void insert(DB *db, struct dbitem *__key, struct dbitem *__item)
{
	db->hdr->count++;
	if(db->hdr->count * 4 >= (1ul << db->hdr->blevel)) {
r:
		//fprintf(stderr, "EXPAND load = %f\n", (float)db->hdr->count / (1ul << db->hdr->blevel));
		db->hdr->blevel++;
		uint64_t bmax = ((1ull << db->hdr->blevel) - 1);
		grow_database(db, ptr_translate(db, TYPE_META, (void *)(bmax * sizeof(struct bucket) + PAGE_SIZE)));
	}
	if(__insert(db, __key, __item) == 0) goto r;
}























static struct dbitem *__loadin(DB *db, DBT *v)
{
	size_t off = db->hdr->dataoff;
	db->hdr->dataoff += ALIGNON(sizeof(struct dbitem) + v->size, 8);
	grow_database(db, ptr_translate(db, TYPE_DATA, (void *)(db->hdr->dataoff)));

	struct dbitem *item = ptr_translate(db, TYPE_DATA, (void *)db->hdr->dataoff);
	item->len = v->size;
	char *vd = v->data;
	for(char *p = (char *)off;p < (char *)(off + v->size);p++) {
		char *vp = ptr_translate(db, TYPE_DATA, p + offsetof(struct dbitem, data));
		*vp = *vd++;
	}
	return ptr_canon(db, TYPE_DATA, item);
}

static int _db_put(DB *db, DB_TXN *txnid, DBT *key, DBT *data, uint32_t flags)
{
	if(flags != 0) {
		return ENOTSUP;
	}
	if(txnid != NULL) return ENOTSUP;
	
	if(lookup(db, key)) return EEXIST;
	struct dbitem *dbik = __loadin(db, key);
	struct dbitem *dbid = __loadin(db, data);
	insert(db, dbik, dbid);
	return 0;
}

static int _db_get(DB *db, DB_TXN *txnid, DBT *key, DBT *data, uint32_t flags)
{
	if(flags != 0) {
		return ENOTSUP;
	}
	if(txnid != NULL) return ENOTSUP;
	struct dbitem *__item = lookup(db, key);
	if(__item == NULL) return ENOENT;
	struct dbitem *item = ptr_translate(db, TYPE_DATA, __item);
	data->data = item->data;
	data->size = item->len;
	return 0;
}

static int _db_del(DB *db, DB_TXN *txnid, DBT *key, uint32_t flags)
{
	if(flags != 0) {
		return ENOTSUP;
	}
	if(txnid != NULL) return ENOTSUP;

	return ENOTSUP;
}

static int _db_close(DB *db, uint32_t flags __unused)
{
	/* TODO: sync? */
	free(db);
	return 0;
}

static int _db_sync(DB *db, uint32_t flags)
{

	return 0;
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

