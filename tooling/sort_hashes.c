#include <stdio.h>
#include <stdlib.h>

#include "hashes.h"
#include "sort.h"
#include "../util.h"

/* sort a file containing uint64_t, in place, in ascending order */

int main(int argc, char **argv)
{
	if (argc < 2) {
		printf("USAGE: %s [filename]\n", argv[0]);
		exit(1);
	}

	uint64_t *A;
	int fd;
	int64_t N;

	mmap_hashes(argv[1], &A, &N, &fd, 1);
	fprintf(stderr, "sorting  %s (in-place)\n", argv[1]);

	double start = wtime();
	sort_list_sequential(A, 0, N);

	int64_t dup = 0;
	for (int64_t i = 1; i < N; i++)
		if (A[i - 1] == A[i]) {
			printf("%ld : %016lx\n", i, A[i]);
			dup++;
		}

	fprintf(stderr, "Done in %.1fs. %ld duplicates\n", wtime() - start, dup);
}
