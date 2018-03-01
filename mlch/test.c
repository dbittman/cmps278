#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define GOLDEN_RATIO_PRIME_64 0x9e37fffffffc0001UL

static uint64_t hash_gr_64(uint64_t val, int bits)
{
	return (val * GOLDEN_RATIO_PRIME_64) >> (64 - bits);
}

static uint64_t hash_djb2_64(uint64_t val, int bits)
{
	uint64_t hash = 5381;
	const unsigned char *data = (const unsigned char *)&val;
	for(int i=0;i<8;i++) {
		hash = ((hash << 5) + hash) ^ *data++;
	}
	return (hash * GOLDEN_RATIO_PRIME_64) >> (64 - bits);
}

#define USED 1
#define SECOND 2

struct bucket {
	void *item;
	uint64_t key;
	uint64_t flags;
};

static int level = 0, minlevel = 0;
static size_t sz;
static struct bucket *buckets;

static long moves = 0, collis = 0;

static void __do_insert(size_t b, uint64_t key, void *item, int fl)
{
	buckets[b].item = item;
	buckets[b].key = key;
	buckets[b].flags = fl;
}

static int __move(size_t b)
{
	uint64_t fl;
	size_t partner;
	struct bucket tmp = buckets[b];
	if(buckets[b].flags == 2) {
		partner = hash_gr_64(buckets[b].key, level);
		fl = 1;
	} else {
		partner = hash_djb2_64(buckets[b].key, level);
		fl = 2;
	}
	//fprintf(stderr, "move: %ld %ld %ld %ld %ld %ld\n", b, buckets[b].flags, partner, buckets[b].key, buckets[partner].key, buckets[partner].flags);
	buckets[b].key = 0;
	buckets[b].flags = 0;
	//fprintf(stderr, "move: %ld: %ld, %ld\n", b, partner, buckets[partner].key);
	if(buckets[partner].flags) {
		__move(partner);
	}
	//fprintf(stderr, "    : %ld: %ld, %ld\n", b, partner, buckets[partner].key);
	if(buckets[partner].flags != 0)
		return 0;
	assert(buckets[partner].flags == 0);
	moves++;
	__do_insert(partner, tmp.key, tmp.item, fl);
	return 1;
}

static int __insert(uint64_t key, void *item)
{
	size_t b1 = hash_gr_64(key, level);
	size_t b2 = hash_djb2_64(key, level);

	//if(buckets[b1].flags) collis++;

	if(buckets[b1].flags == 0) {
		__do_insert(b1, key, item, 1);
	} else if(buckets[b2].flags == 0) {
		__do_insert(b2, key, item, 2);
	} else if(buckets[b1].flags == 1) {
		if(__move(b1) == 0) goto a;
		__do_insert(b1, key, item, 1);
	} else if(buckets[b2].flags == 1) {
a:
		if(__move(b2) == 0) return 0;
		__do_insert(b2, key, item, 1);
	} else {
		if(__move(b1) == 0) return 0;
		__do_insert(b1, key, item, 1);
	}
	return 1;
}

static long rehashes = 0;
static long collis_re = 0, moves_re = 0;
static long updates_re = 0;
static void rehash(int newlevel)
{
	long _c = collis;
	long _m = moves;

	rehashes++;
	struct bucket *old = malloc(sz * sizeof(struct bucket));
	memcpy(old, buckets, sz * sizeof(struct bucket));

	size_t oldsz = sz;
	level = newlevel;
	sz = (1 << level);
	buckets = realloc(buckets, sz * sizeof(struct bucket));
	for(size_t i=0;i<sz;i++) buckets[i].flags = 0;
	for(size_t i=0;i<oldsz;i++) {
		if(old[i].flags) {
			updates_re++;
			__insert(old[i].key, old[i].item);
		}
	}
	collis_re += collis - _c;
	moves_re += moves - _m;
	collis = _c;
	moves = _m;
}

static void *__lookup(uint64_t key, int lev)
{
	size_t b1 = hash_gr_64(key, lev);
	size_t b2 = hash_djb2_64(key, lev);

	if(buckets[b1].key == key && buckets[b1].flags) {
		return buckets[b1].item;
	} else if(buckets[b2].key == key && buckets[b2].flags) {
		return buckets[b2].item;
	}
	return NULL;
}

static size_t count = 0;
static long lookups = 0;


static void *lookup_w(uint64_t key)
{
	void *ret;
	for(int i=minlevel;i<=level;i++) {
		lookups++;
		if((ret=__lookup(key, i))) return ret;
	}
	return NULL;
}

static void insert_w(uint64_t key, void *item)
{
	count++;
	if(lookup_w(key)) return;
	if(count * 4 >= sz) {
r:
		level++;
		size_t oldsz = sz;
		sz = (1 << level);
		buckets = realloc(buckets, sz * sizeof(struct bucket));
		for(size_t i=oldsz;i<sz;i++) buckets[i].flags = 0;

	}
	if(__insert(key, item) == 0) goto r;
}




static void *lookup(uint64_t key)
{
	lookups++;
	return __lookup(key, level);
}

static void insert(uint64_t key, void *item)
{
	count++;
	if(lookup(key)) return;
	if(count * 4 >= sz) {
r:
		rehash(level+1);
	}
	if(__insert(key, item) == 0) goto r;
}


int main(int argc, char **argv)
{
	srandom(atoi(argv[1]));
	int total = atoi(argv[2]);
	int w = argv[3][0] == 'w';

	minlevel = level = 8;
	sz = (1 << level);
	buckets = calloc(sz, sizeof(struct bucket));

	for(int i=0;i<total;i++) {
		unsigned long k = random();
		if(w) insert_w(k, (void *)1);
		else insert(k, (void *)1);
	}

	fprintf(stderr, "%s:    COUNT     SIZE   COLLIS  LOOKUPS REHASHES    MOVES     UPRE   COLLRE   MOVERE\n", w ? "new   " : "old   ");
	fprintf(stderr, "RESULT: %8ld %8ld %8ld %8ld %8ld %8ld %8ld %8ld %8ld\n",
			count, sz, collis, lookups, rehashes, moves, updates_re, collis_re, moves_re);

}

