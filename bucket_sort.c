#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "util.h"
#include "bucket_sort.h"


/* Sort L according to the 8*passes most significant bits, i.e. bits [64-8*passes:64].
 * The input L is destroyed.
 * M must be prallocated of size N. returns a pointer to the output (either L or M).
 */
uint64_t * radix256_sort(uint64_t *L, uint64_t *M, int64_t N, int passes)
{
	assert(passes == 2 || passes == 3 || passes == 4);
	assert(N < (1ll << 32));

	uint32_t COUNT[4][256];

	uint64_t *IN = L;
	uint64_t *OUT = M;

	/* setup */
	for (int k = 4-passes; k < 4; k++)
		for (int i = 0; i < 256; i++)
			COUNT[k][i] = 0;

	/* multi-histogramming: one pass for all counts */
	if (passes == 2)
		for (int64_t i = 0; i < N; i++) {
			COUNT[2][(IN[i] >> 48) & 0xff] += 1;
			COUNT[3][IN[i] >> 56] += 1;
		}
	if (passes == 3)
		for (int64_t i = 0; i < N; i++) {
			COUNT[1][(IN[i] >> 40) & 0xff] += 1;
			COUNT[2][(IN[i] >> 48) & 0xff] += 1;
			COUNT[3][IN[i] >> 56] += 1;
		}		
	if (passes == 4)
		for (int64_t i = 0; i < N; i++) {
			COUNT[0][(IN[i] >> 32) & 0xff] += 1;
			COUNT[1][(IN[i] >> 40) & 0xff] += 1;
			COUNT[2][(IN[i] >> 48) & 0xff] += 1;
			COUNT[3][IN[i] >> 56] += 1;
		}		

	/* prefix-sum */
	for (int k = 4-passes; k < 4; k++)
		for (int i = 1; i < 256; i++)
			COUNT[k][i] += COUNT[k][i - 1];
	
	/* dispatch : */
	uint64_t *tmp;
	
	if (passes >= 4) {
		for (int64_t i = N - 1; i >= 0; i--)
			OUT[--COUNT[0][(IN[i] >> 32) & 0xff]] = IN[i];
		tmp = OUT; OUT = IN; IN = tmp;
	}
	if (passes >= 3) {
		for (int64_t i = N - 1; i >= 0; i--)
			OUT[--COUNT[1][(IN[i] >> 40) & 0xff]] = IN[i];
		tmp = OUT; OUT = IN; IN = tmp;
	}
	for (int64_t i = N - 1; i >= 0; i--)
		OUT[--COUNT[2][(IN[i] >> 48) & 0xff]] = IN[i];
	tmp = OUT; OUT = IN; IN = tmp;
	
	for (int64_t i = N - 1; i >= 0; i--)
		OUT[--COUNT[3][IN[i] >> 56]] = IN[i];
	return OUT;
}


/* determine the bucket indices for an already-sorted array, with 2^k buckets */
int64_t* bucketize(uint64_t *L, int64_t N, int k)
{
	int64_t *Ap = malloc(((1ull << k) + 1) * sizeof(*Ap));
	Ap[0] = 0;
	uint64_t prefix = 0;  /* current prefix */
	uint64_t next = 1;     /* next prefix */
	for (int64_t i = 0; i < N; i++)
		if (MSB(L[i], k) > prefix) {
			prefix = MSB(L[i], k);
			while (next <= prefix)
				Ap[next++] = i;
		}
	while (next <= (1ull << k))
		Ap[next++] = N;
	return Ap;
}