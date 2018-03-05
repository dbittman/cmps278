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


	uint64_t count =     1000000;
	for(uint64_t i = 0;i<count;i++) {
		DBT key, data;
		memset(&key, 0, sizeof(key));
		memset(&data, 0, sizeof(data));

		key.data = &i;
		key.size = sizeof(i);
		data.data = &i;
		data.size = sizeof(i);

		if((ret = dbp->put(dbp, NULL, &key, &data, 0)) == 0)
			fprintf(stderr, "db: %lx: key stored.\n", *(uint64_t *)key.data);
		else {
			dbp->err(dbp, ret, "DB->put");
			exit(1);
		}
		memset(&data, 0, sizeof(data));
		if((ret = dbp->get(dbp, NULL, &key, &data, 0)) == 0);
	//		printf("db: %lx: key retrieved: data was %lx.\n",
	//				*(uint64_t *)key.data, *(uint64_t *)data.data);
		else {
			dbp->err(dbp, ret, "DB->get");
			exit(1);
		}
	}

	for(uint64_t i = 0;i<count;i++) {
		DBT key, data;
		memset(&key, 0, sizeof(key));
		memset(&data, 0, sizeof(data));

		key.data = &i;
		key.size = sizeof(i);
		data.data = &i;
		data.size = sizeof(i);

		memset(&data, 0, sizeof(data));
		if((ret = dbp->get(dbp, NULL, &key, &data, 0)) != 0) {
			dbp->err(dbp, ret, "DB->get %lx", i);
			exit(1);
		}
	}

}

