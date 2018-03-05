#pragma once

struct nvkv;
typedef struct nvkv DB;
struct kvt {
	void *data;
	size_t size;
};

typedef enum dbtype {
	DB_HASH,
} DBTYPE;

struct dbtx {
	int a;
};

struct dbenv {
	int a;
};

typedef struct dbtx DB_TXN;
typedef struct dbenv DB_ENV;
typedef struct kvt DBT;

struct dbitem {
	uint64_t len;
	unsigned char data[];
} __packed;

struct bucket {
	struct dbitem *kp, *dp;
	uint64_t flags;
	uint64_t pad;
} __packed;

#define DB_NOOVERWRITE 1

#define _NVKV_MAGIC 0x332994ab
struct dbheader {
	uint32_t magic;
	uint16_t flags;
	uint8_t type;
	uint8_t pad0;

	uint64_t count;
	uint32_t blevel, minlevel;
	uint64_t dataoff;
} __packed;

struct nvkv {
	int (*open)(DB *db, DB_TXN *txnid, const char *file, 
			const char *database, DBTYPE type, uint32_t flags, int mode);
	int (*put)(DB *db, DB_TXN *txnid, DBT *key, DBT *data, uint32_t flags);
	int (*get)(DB *db, DB_TXN *txnid, DBT *key, DBT *data, uint32_t flags);
	int (*del)(DB *db, DB_TXN *txnid, DBT *key, uint32_t flags);
	int (*close)(DB *db, uint32_t flags);
	int (*sync)(DB *db, uint32_t flags);
	void (*err)(DB *db, int error, const char *fmt, ...);
	void (*errx)(DB *db, const char *fmt, ...);

	void *base;
	int fd;
	struct dbheader *hdr;
	size_t size;
};

#define DB_CREATE   1
#define DB_EXCL     2
#define DB_TRUNCATE 4


#define DB_UNKNOWN 0
#define DB_HASH    1

int db_create(DB **dbp, DB_ENV *dbenv, uint32_t flags);

#include <errno.h>
#include <string.h>
#define db_strerror strerror

