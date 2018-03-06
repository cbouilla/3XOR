#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "util.h"
#include "membership.h"

extern inline uint64_t H_1(struct cuckoo_hash_t *s, uint64_t h);
extern inline uint64_t H_2(struct cuckoo_hash_t *s, uint64_t h);

/* initialize a hash table with entries from L[lo:hi][0:n] */
struct cuckoo_hash_t *init_cuckoo_hash(uint64_t * const L, size_t lo, size_t hi)
{
	struct cuckoo_hash_t *s = malloc(sizeof(struct cuckoo_hash_t));
	size_t tmp = hi-lo; //4 * (hi - lo) / 3;
	int i = 0;
	while (tmp) {
		tmp >>= 1;
		i++;
	}
	if (i < L1_CACHE_SIZE_LOG2 - 5)
		i = L1_CACHE_SIZE_LOG2 - 5;
	uint64_t size = 1 << i;
	s->mask = RIGHT_MASK(i);;
	s->multiplier = 3;
	s->G = malloc(size * sizeof(uint64_t));
	s->H = malloc(size * sizeof(uint64_t));

	assert(size < 65536);
	// fprintf(stderr, "Allocating %zd bytes (fill=%.1f%%), |L|=%zd\n", 16 * size, (100.0 * (hi - lo)) / size, hi - lo);

	/* selects the multiplier */
	int64_t trials = 0;
	int verbose = 0;
	while (1) {
 start:
 		trials++;
		 if (trials > 16)
		   verbose = 1;
		for (uint64_t i = 0; i < size; i++) {
			s->G[i] = 0;
			s->H[i] = 0;
		}

		for (uint64_t i = lo; i < hi; i++) {
			uint64_t x = L[i];
			assert(x);
			int side = 0;
			int loops = 0;
			int ok = 0;

			while (!ok && loops < 1000) {
				uint64_t y;
				if (side == 0) {
					uint64_t h = H_1(s, x);
					y = s->H[h];
					s->H[h] = x;
				} else {
					uint64_t h = H_2(s, x);
					y = s->G[h];
					s->G[h] = x;
				}
				ok = (y == 0 || x == y);
				x = y;
				side = 1 - side;
				loops++;
			}
			if (ok)
				continue;

			/* insertion failed */
			if (verbose)
				fprintf(stderr, "changing multiplier (problem with %016lx) @i=%ld\n", L[i], i);
			s->multiplier = ((rand() << 31) + rand());
			goto start;
		}
		return s;
	}
}

void free_cuckoo_hash(struct cuckoo_hash_t *s)
{
	free(s->H);
	free(s->G);
	free(s);
}

/*****************************************************************************************/

struct linear_hash_t *init_linear_hash(uint64_t * const L, size_t lo, size_t hi)
{
	struct linear_hash_t *s = malloc(sizeof(struct linear_hash_t));
	int i = 1;
	uint64_t tmp = hi - lo;
	while (tmp) {
		tmp >>= 1;
		i++;
	}
	while (1.0 * (hi - lo) / (1 << i) > 0.125)
		i++;

	uint64_t size = 1 << i;

	printf("allocating %zu bytes, load factor = %.1f\n", 8 * size,
	       (1.0 * size) / (hi - lo));

	s->mask = size - 1;
	s->H = malloc(size * sizeof(uint64_t));

	assert(size < 1000000);
	// fprintf(stdout, "Allocating %zd bytes (fill=%.1f%%), i=%d, mask = %016" PRIx64 ", shift = %d, size=%zd\n", 16*s->size, (100.0 * (hi-lo)) / s->size,  i, s->mask, s->shift, hi-lo);

	for (size_t i = 0; i < size; i++)
		s->H[i] = 0;

	for (size_t i = lo; i < hi; i++) {
		assert(L[i]);
		uint64_t h = L[i] & s->mask;
		while (s->H[h])
			h = (h + 13) & s->mask;
		s->H[h] = L[i];
	}
	return s;
}

void free_linear_hash(struct linear_hash_t *s)
{
	free(s->H);
	free(s);
}
