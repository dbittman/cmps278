#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <err.h>

#if _USE_BDB
#include <db.h>
#define DATABASE "test.db"
#else
#include "nvkv.h"
#define DATABASE "test.nvdb"
#endif

int main()
{
	DB *dbp;
	int ret;

	system("rm " DATABASE);

	if((ret = db_create(&dbp, NULL, 0)) != 0) {
		fprintf(stderr, "db_create: %s\n", db_strerror(ret));
		exit(1);
	}
	
	if((ret = dbp->open(dbp, NULL, DATABASE, NULL, DB_HASH, DB_CREATE, 0664)) != 0) {
		dbp->err(dbp, ret, "%s", DATABASE);
		exit(1);
	}


	DBT key, data;
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	key.data = "fruit";
	key.size = sizeof("fruit");
	data.data = "apple";
	data.size = sizeof("apple");

	if ((ret = dbp->put(dbp, NULL, &key, &data, 0)) == 0)
		printf("db: %s: key stored.\n", (char *)key.data);
	else {
		dbp->err(dbp, ret, "DB->put");
		exit(1);
	}
}

