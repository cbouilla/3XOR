#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include "tests.h"
#include "../tooling/sort.h"

/* produce 3 hash files with 3 solutions, to test correctness and speed of the rest. */

void save_hashes(char *filename, uint64_t *A, int64_t N)
{
	FILE *file = fopen(filename, "w");
	size_t check = fwrite(A, sizeof(*A), N, file);
	fclose(file);

	fprintf(stderr, "Wrote %zd bytes to %s\n", check*sizeof(*A), filename);
}

int main(int argc, char **argv) {
	
	if (argc < 5) {
		printf("USAGE: %s [foo.bin] [bar.bin] [foobar.bin] [N]\n", argv[0]);
		exit(1);
	}

	int64_t N = atoll(argv[4]);

	uint64_t *A = list_rand_init(N, 64, 0);
	uint64_t *B = list_rand_init(N, 64, 1);
	uint64_t *C = list_rand_init(N, 64, 2);

	sort_list_sequential(A, 0, N);
	sort_list_sequential(B, 0, N);

	srand(0);
	C[0] = A[0] ^ B[0];
	C[N-1] = A[N - 1] ^ B[N - 1];
	int64_t i = rand() % N;
	int64_t j = rand() % N;
	int64_t k = rand() % N;
	C[i] = A[j] ^ B[k];
	uint64_t a = C[0];
	uint64_t b = C[N-1];
	uint64_t c = C[i];

	sort_list_sequential(C, 0, N);

	int64_t x = -1, y = -1, z = -1;
	for (int64_t u = 0; u < N; u++) {
		if (C[u] == a)
			x = u;
		if (C[u] == b)
			y = u;
		if (C[u] == c)
			z = u;
	}

	assert(C[x] == (A[0] ^ B[0]));
	assert(C[y] == (A[N-1] ^ B[N-1]));
	assert(C[z] == (A[j] ^ B[k]));

	printf("Expected solutions :\n");
	printf("(0, 0, %ld) : %016lx ^ %016lx ^ %016lx = 0\n", x, A[0], B[0], C[x]);
	printf("(%ld, %ld, %ld) : %016lx ^ %016lx ^ %016lx = 0\n", N-1, N-1, y, A[N-1], B[N-1], C[y]);
	printf("(%ld, %ld, %ld) : %016lx ^ %016lx ^ %016lx = 0\n", j, k, z, A[j], B[k], C[z]);
	
	save_hashes(argv[1], A, N);
	save_hashes(argv[2], B, N);
	save_hashes(argv[3], C, N);
}