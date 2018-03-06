#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <getopt.h>

#include "shares.h"

int share_cmp(const void *a_, const void *b_)
{
	const struct share_t *a = a_;
	const struct share_t *b = b_;

	if (a->extraNonce2 < b->extraNonce2) return -1;
	if (a->extraNonce2 > b->extraNonce2) return +1;
	if (a->extraNonce1 < b->extraNonce1) return -1;
	if (a->extraNonce1 > b->extraNonce1) return +1;
	if (a->nonce < b->nonce) return -1;
	if (a->nonce > b->nonce) return +1;
	if (a->ntime < b->ntime) return -1;		
	if (a->ntime > b->ntime) return +1;
	if (a->kind < b->kind) return -1;
	if (a->kind > b->kind) return +1;
	
	return 0;
}

void process_file(char *filename, int purge)
{
	struct share_t *SHARES = NULL;
	int64_t N = 0;
	int fd = -1;
	mmap_shares(filename, &SHARES, &N, &fd);
	printf("Loading %ld shares in memory (%.1fMbyte)\n", N, N * 1.9073486328125e-05);
	struct share_t *shares = malloc(N * sizeof(*shares));
       	for (int64_t i = 0; i < N; i++)
       		shares[i] = SHARES[i];
       	// memcpy(shares, SHARES, N * sizeof(*shares));
	munmap_shares(SHARES, N, fd);

	printf("Sorting\n");
	qsort(shares, N, sizeof(*shares), share_cmp);

	printf("Uniqueing\n");
	int64_t dup = 0;
	int64_t prev = 0;
	for (int64_t i = 1; i < N; i++) {
		int outcome = share_cmp(&shares[prev], &shares[i]);
		assert(outcome <= 0);
		if (outcome == 0) {
			dup += 1;
			//if (!out)
			//	print_share(&shares[prev]);
			//print_share(&shares[i]);
		}
		else {
			prev = i;
		}
	}

	if (dup == 0) {
		printf("%s : no duplicate share found.\n", filename); 
		goto end;
	}
	
	if (!purge) {
		printf("%s : %ld duplicate shares found. Run with --purge to actually deduplicate (have a backup)\n", filename, dup);
		goto end;
	}
	
	printf("%s : %ld duplicate shares found. Rewriting file.\n", filename, dup);
	/* dedup */
	struct share_t *unique = malloc((N - dup) * sizeof(*unique));
	unique[0] = shares[0];
	int64_t j = 0;
	for (int64_t i = 1; i < N; i++)
		if (share_cmp(unique + j, shares + i) != 0)
			unique[++j] = shares[i]	;
	assert(j == N - dup - 1);

	for (int64_t i = 1; i < N - dup; i++)
		assert(share_cmp(unique + (i - 1), unique + i) < 0);

	for (int64_t i = 1; i < N - dup; i++) {
		assert((unique[i-1].nonce != unique[i].nonce)
			|| (unique[i-1].ntime != unique[i].ntime)
			|| (unique[i-1].extraNonce1 != unique[i].extraNonce1)
			|| (unique[i-1].extraNonce2 != unique[i].extraNonce2)
			|| (unique[i-1].kind != unique[i].kind));
	}


	/* save */
	assert(sizeof(*unique) == 20);
	FILE *share_file = fopen(filename, "w");
	assert(share_file != NULL);
	size_t check = fwrite(unique, sizeof(*unique), N - dup, share_file);
	assert(check == (size_t) ((N - dup)));
	fclose(share_file);
	free(unique);

	end:
	free(shares);
}


int main(int argc, char **argv)
{
	assert(sizeof(struct share_t) == 20);
	struct option longopts[2] = {
		{"purge", no_argument, NULL, 'p'},
		{NULL, 0, NULL, 0}
	};

	int purge = 0;
	char ch;
	while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
	    	switch (ch) {
		case 'p':
			purge = 1;
			break;
		default:
			printf("Unknown option\n");
			exit(EXIT_FAILURE);
		}
  	}

	for (int i = optind; i < argc; i++)
                process_file(argv[i], purge);

        exit(EXIT_SUCCESS);
}
