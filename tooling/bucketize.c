#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>


#include "hashes.h"
#include "index.h"
#include "../bucket_sort.h"

/* given a prefix width, find the bucket limits and write them down to a file */

void bucketize_file(char *filename, int prefix_width)
{
	uint64_t *L = NULL;
	int64_t N = 0;
	int fd = -1;
	mmap_hashes(filename, &L, &N, &fd, 0);
	int64_t *U = bucketize(L, N, prefix_width);

	save_index(filename, U, prefix_width);
	munmap_hashes(L, N, fd);

	int64_t thin = 0;
	int64_t fat = 0;
	int64_t avg = N / (1 << prefix_width);
	for(int64_t i = 0; i < (1 << prefix_width); i++) {
		int64_t size = U[i+1] - U[i];
		if (size < avg/3)
			thin++;
		if (size > 3*avg)
			fat++;
	}
	printf("%s: %ld buckets, avg size %ld, %ld thin, %ld fat\n", filename, 1l << prefix_width, avg, thin, fat);
	free(U);
}

void advise_file(char *filename)
{
	uint64_t *L = NULL;
	int64_t N = 0;
	int fd = -1;
	mmap_hashes(filename, &L, &N, &fd, 0);
	
	int L1_size = 15;
	int pw = 0;
	while (N / (1ll << pw) > (1 << (L1_size - 7)))
		pw++;

	printf("%s: suggesting --prefix-width=%d --> avg bucket size %ld\n", filename, pw, N / (1l << pw));
	munmap_hashes(L, N, fd);
}

int main(int argc, char **argv)
{
	struct option longopts[3] = {
		{"prefix-width", required_argument, NULL, 'p'},
		{"advise", no_argument, NULL, 'a'},
		{NULL, 0, NULL, 0}
	};

	int prefix_width = -1;
	int advise = 0;
	char ch;
	while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
	    	switch (ch) {
		case 'p':
			prefix_width = atoi(optarg);
			break;
		case 'a':
			advise = 1;
			break;
		default:
			printf("Unknown option\n");
			exit(EXIT_FAILURE);
		}
  	}

	if ((!advise && prefix_width < 0) || optind >= argc) {
		printf("USAGE: %s [--prefix-width=WIDTH] [--advise] hash_file1 [hash_file2] [hash_file3]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* bucketize */
	while (optind < argc)
                if (advise)
                	advise_file(argv[optind++]);
                else
                	bucketize_file(argv[optind++], prefix_width);
	
}