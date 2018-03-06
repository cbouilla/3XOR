#include <stdio.h>
#include <assert.h>
#include <getopt.h>
#include <math.h>


#include "shares.h"
#include "hashes.h"
#include "../util.h"

double total_eq1[3] = {0, 0, 0};
int64_t total_kinds[3] = {0, 0, 0};


void process_file(char *filename, char *hash_dir)
{
	(void)(hash_dir);
	struct share_t *SHARES = NULL;
	int64_t N = 0;
	int fd = -1;
	mmap_shares(filename, &SHARES, &N, &fd);

	uint64_t *hashes[3];
	for (int i = 0; i < 3; i++)
		hashes[i] = malloc(sizeof(uint64_t) * N);
	int64_t kinds[3] = {0, 0, 0};

	#pragma omp parallel
	{
		struct block_context_t *ctx = init_block_context();

		#pragma omp for
		for (int64_t i = 0; i < N; i++) {
			uint32_t block[20], hash[8];
			assemble_block(block, &SHARES[i], ctx);
			hash_block(hash, block);
			assert(hash[7] == 0);

			if (tid() == 0 && (i & 0xffff) == 0) {
				double bits = log2(total_eq1[0]) + log2(total_eq1[1]) + log2(total_eq1[2]) + 32;
				printf("%s : %ld : [%ld, %ld, %ld] --> %.1f bits\r", filename, N, kinds[0], kinds[1], kinds[2], bits);
				fflush(stdout);
			}
			int kind = SHARES[i].kind;
			int diff = SHARES[i].difficulty;

			int64_t idx;
			#pragma omp atomic capture
			idx = kinds[kind]++;

			char *h = (char *) hash;
			uint64_t *hh = (uint64_t *) (h + 19);
			hashes[kind][idx] = *hh;

			#pragma omp atomic update
			total_eq1[kind] += pow(diff, 1. / 3.);
		}
		free(ctx);
	}
	printf("\n");
	munmap_shares(SHARES, N, fd);
	for (int i = 0; i < 3; i++) {
		total_kinds[i] += kinds[i];
		save_hashes(filename, hash_dir, i, hashes[i], kinds[i]);
		free(hashes[i]);
	}
}


int main(int argc, char **argv)
{
	/* options descriptor */
	struct option longopts[2] = {
		{"hash-directory", required_argument, NULL, 'd'},
		{NULL, 0, NULL, 0}
	};

	char *hash_dir = NULL;
	char ch;
	while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
	    	switch (ch) {
		case 'd':
			hash_dir = optarg;
			break;
		default:
			printf("Unknown option\n");
			exit(EXIT_FAILURE);
		}
  	}

	if (!hash_dir || optind >= argc) {
		printf("USAGE: %s --hash-directory=... block_file1 [block_file2] [block_file3]...\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	while (optind < argc)
                process_file(argv[optind++], hash_dir);

	double bits = log2(total_eq1[0]) + log2(total_eq1[1]) + log2(total_eq1[2]) + 32;
	printf("\n");
	printf("TOTAL : [%ld, %ld, %ld] --> %.1f bits\n", total_kinds[0], total_kinds[1], total_kinds[2], bits);

        exit(EXIT_SUCCESS);
}