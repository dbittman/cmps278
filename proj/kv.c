#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <sys/mman.h>
#include "nvkv.h"

static uintptr_t _ptr_translate(DB *db, uintptr_t offset)
{
	return (uintptr_t)db->base + offset;
}

static uintptr_t _ptr_canon(DB *db, uintptr_t ptr)
{
	return ptr - (uintptr_t)db->base;
}

#define ptr_translate(db, ptr) \
	({ (typeof(ptr))_ptr_translate(db,(uintptr_t)(ptr)); })

#define ptr_canon(db, ptr) \
	({ (typeof(ptr))_ptr_canon(db,(uintptr_t)(ptr)); })




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

static int _db_put(DB *db, DB_TXN *txnid, DBT *key, DBT *data, uint32_t flags)
{

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

