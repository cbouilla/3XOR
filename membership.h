#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>

/* inline functions for fast membership testing */
struct cuckoo_hash_t {
	uint64_t mask;  // size - 1
	uint64_t multiplier;
	uint64_t *H;  // hash table
	uint64_t *G;  // hash table
};


struct linear_hash_t {
	uint64_t mask;  // size - 1
	uint64_t *H;    // hash table
};

struct cuckoo_hash_t *init_cuckoo_hash(uint64_t * const L, size_t lo, size_t hi);
void free_cuckoo_hash(struct cuckoo_hash_t *s);
struct linear_hash_t *init_linear_hash(uint64_t * const L, size_t lo, size_t hi);
void free_linear_hash(struct linear_hash_t *s);

inline uint64_t H_1(struct cuckoo_hash_t *s, uint64_t h)
{
	return h & s->mask;
}

inline uint64_t H_2(struct cuckoo_hash_t *s, uint64_t h)
{
	uint64_t p = h * s->multiplier;
	// return ((p & 0xffff) - (p >> 16)) & s->mask;
	return (p % 65537) & s->mask;
}

/* return 1 if x belongs to the datastucture. */
static inline int cuckoo_lookup(struct cuckoo_hash_t *s, uint64_t x)
{
	uint64_t hash_1 = H_1(s, x);
	uint64_t probe_1 = s->H[hash_1];
	
	
	if (probe_1 == 0)
		return 0;
	
	if (probe_1 == x)
		return 1;
	
	uint64_t hash_2 = H_2(s, x);
	uint64_t probe_2 = s->G[hash_2];

	return (probe_2 == x);
}

/* return 1 if x belongs to the datastucture. */
static inline int linear_lookup(struct linear_hash_t *s, uint64_t x)
{
	uint64_t h = x & s->mask;
	uint64_t probe = s->H[h];
	while (probe) {
		if (probe == x)
			return 1;
		h = (h + 13) & s->mask;
		probe = s->H[h];
	}
	return 0;
}


