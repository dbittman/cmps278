#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <err.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

#if _USE_BDB
#include <db.h>
#define DATABASE "test.db"
#define _DB_LIB "bdb"
#else
#include "nvkv.h"
#define DATABASE "test.nvdb"
#define _DB_LIB "nvkv"
#endif

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

void bench(DB *dbp)
{
	int ret;
	struct timespec start, end, diff;
	uint64_t count = 1000000;
	for(uint64_t i = 0;i<count;i++) {
		DBT key, data;
		memset(&key, 0, sizeof(key));
		memset(&data, 0, sizeof(data));

		key.data = &i;
		key.size = sizeof(i);
		data.data = &i;
		data.size = sizeof(i);

		clock_gettime(CLOCK_MONOTONIC, &start);
		dbp->put(dbp, NULL, &key, &data, 0);
		clock_gettime(CLOCK_MONOTONIC, &end);
		timespec_diff(&start, &end, &diff);
		fprintf(stderr, "BENCH " _DB_LIB " put %ld\n", diff.tv_nsec + diff.tv_sec * 1000000000);
	}

	for(uint64_t i = 0;i<count*2;i++) {
		DBT key, data;
		memset(&key, 0, sizeof(key));
		memset(&data, 0, sizeof(data));

		key.data = &i;
		key.size = sizeof(i);
		clock_gettime(CLOCK_MONOTONIC, &start);
		ret = dbp->get(dbp, NULL, &key, &data, 0);
		clock_gettime(CLOCK_MONOTONIC, &end);
		timespec_diff(&start, &end, &diff);
		fprintf(stderr, "BENCH " _DB_LIB " get %s %ld\n",
				ret == 0 ? "p" : "np", diff.tv_nsec + diff.tv_sec * 1000000000);
	}

	for(uint64_t i = 0;i<count;i++) {
		DBT key, data;
		memset(&key, 0, sizeof(key));
		memset(&data, 0, sizeof(data));

		uint64_t k = 1024;
		key.data = &k;
		key.size = sizeof(k);
		clock_gettime(CLOCK_MONOTONIC, &start);
		ret = dbp->get(dbp, NULL, &key, &data, 0);
		clock_gettime(CLOCK_MONOTONIC, &end);
		timespec_diff(&start, &end, &diff);
		fprintf(stderr, "BENCH " _DB_LIB " get hot %ld\n",
				diff.tv_nsec + diff.tv_sec * 1000000000);
	}

}

int test(DB *dbp)
{
	int ret;
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
			printf("db: %lx: key stored.\n", *(uint64_t *)key.data);
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

	return 0;
}

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



	bench(dbp);
	return 0;
}

