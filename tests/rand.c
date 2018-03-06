#include <inttypes.h>
#include <stdlib.h>

#include "tests.h"
#include "../util.h"
#include "../linalg.h"

/* Allocate a list of N random n-bit vectors. Bits [0:n] are random, bits [n:64] are zero. 
 * Multithreaded and deterministic. 
 */
uint64_t *list_rand_init(int64_t N, unsigned int n, uint64_t seed)
{
	uint64_t *L = malloc(N * sizeof(*L));
	uint64_t chacha_key[2] = {seed, 0};

	/* generate random values, 8Mb at a time. Deterministic. */
	int64_t block_size = 1024*1024;
	#pragma omp parallel for schedule(static)
	for (int64_t i = 0; i < N / block_size; i++) {
		ECRYPT_ctx chacha_state;
		ECRYPT_keysetup(&chacha_state, (uint8_t *) chacha_key, 128, 64);
		ECRYPT_ivsetup(&chacha_state, (uint8_t *) &i);
		ECRYPT_keystream_bytes(&chacha_state, (uint8_t *) &L[i * block_size], block_size * sizeof(uint64_t));
	}
	
	/* last (incomplete) pass */
	int64_t last_offset = (N / block_size) * block_size;
	int64_t last_size = sizeof(uint64_t) * (N - last_offset);
	
	ECRYPT_ctx chacha_state;
	ECRYPT_keysetup(&chacha_state, (uint8_t *) chacha_key, 128, 64);
	ECRYPT_ivsetup(&chacha_state, (uint8_t *) &last_offset);
	ECRYPT_keystream_bytes(&chacha_state, (uint8_t *) &L[last_offset], last_size);
	
	if (n == 64)
		return L;
	
	/* keep only the n highest significant bits */
	uint64_t mask = RIGHT_MASK(n);
	#pragma omp parallel for schedule(static)
	for (int64_t i = 0; i < N; i++)
		L[i] &= mask;
	return L;
}



void matmul_rand_init(struct matmul_table_t* T, uint64_t seed)
{
	uint64_t *M = list_rand_init(8*256, 64, seed);

	for (int64_t i = 0; i < 8; i++) 
		for (int64_t j = 0; j < 256; j++)
			T->tables[i][j] = M[i*256 + j];

	free(M);
}