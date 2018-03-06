#include <inttypes.h>
#include <stddef.h>


#define L1_CACHE_SIZE_LOG2 15

static inline uint64_t LEFT_MASK(unsigned int n)
{
	return ~((1ull << (64ull - n)) - 1);
}

static inline uint64_t RIGHT_MASK(unsigned int n)
{
	return (n == 64) ? 0xffffffffffffffffull : ((1ull << n)) - 1;
}

static inline uint64_t MSB(uint64_t x, int k)
{
	if (k <= 0)
		return 0;
	return (x >> (64ll - k));
}

static inline uint64_t ROL(uint64_t x, unsigned int k)
{
	return (x << k) ^ (x >> (64 - k));
}

static inline void SWAP(uint64_t *A, size_t i, size_t j)
{
	uint64_t tmp = A[i];
	A[i] = A[j];
	A[j] = tmp;
}

// #define MAX(a,b) (((a) > (b)) ? (a) : (b))
/* utils.c */

uint64_t rdtsc();
double wtime();
int tid();
int num_threads();
int max_threads();
int num_places();
int place_num();
int parallel_level();