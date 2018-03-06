#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <err.h>
#include <inttypes.h>
#include <getopt.h>
#include <string.h>

#include "../util.h" 
#include "../client_server/server.h" 
#include "../client_server/tasks.h" 
#include "shares.h" 
#include "sort.h" 


int64_t matches[3] = {0, 0, 0};

/* return i such that C[i] == target, or -1 if not found */
int64_t binary_search(uint64_t *C, int64_t lo, int64_t hi, uint64_t target)
{
	int64_t hi0 = hi;
	while (lo < hi) {
		int64_t mid = (lo + hi) / 2;
		if (C[mid] < target)
			lo = mid + 1;
		else
			hi = mid;
	}
	return (lo < hi0 && C[lo] == target) ? lo : -1;
}


int64_t load_solutions(const char *filename, uint64_t **H)
{
	printf("*) Reading match journal file %s\n", filename);
	FILE *log_file = fopen(filename, "r");
	if (!log_file)
		err(1, "opening %s", filename);
	
	int64_t total_solutions = 0;
	double total_time = 0;
	int64_t tasks_done = 0;
	int64_t room = 1000;
	for (int i = 0; i < 2; i++)
		H[i] = malloc(sizeof(uint64_t) * room);
	
	while (!feof(log_file)) {
		struct record_t record;
		if (1 != fread(&record, sizeof(struct record_t), 1, log_file)) {
			if (feof(log_file))
				break;
			err(1, "reading task record");
		}
		total_time += record.duration;

		char payload[record.size];
		if ((size_t) record.size != fread(payload, 1, record.size, log_file))
			err(1, "reading payload");

		struct message_t *msg = (struct message_t *) payload;
		assert(msg->id == record.id);
		assert((msg->result_size % 16) == 0);
		int size = msg->result_size / 16;

		uint64_t *result = (uint64_t *) (msg->data + msg->hostname_length);

		if (total_solutions + size >= room) {
			room = room * 2;
			for (int i = 0; i < 2; i++)
				H[i] = realloc(H[i], sizeof(uint64_t) * room);
		}
		for (int64_t i = 0; i < size; i++) {
			H[0][total_solutions + i] = result[2 * i];
			H[1][total_solutions + i] = result[2 * i + 1];
		}
		total_solutions += size;
		tasks_done++;
	}
	fclose(log_file);

	printf("%zd tasks done, %zd potential solutions found\n", tasks_done, total_solutions);
	printf("Total sequential time: %.0fs\n", total_time);

	H[2] = malloc(total_solutions * sizeof(uint64_t));
	for (int64_t i = 0; i < total_solutions; i++)
		H[2][i] = H[0][i] ^ H[1][i];
	return total_solutions;
}



int P[6][3] = { {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
/*
  foo bar foobar ---> permutation = 0
  foo foobar bar ---> permutation = 1
  bar foo foobar ---> permutation = 2
  foobar foo bar ---> permutation = 3
  bar foobar foo ---> permutation = 4  
  foobar bar foo ---> permutation = 5
*/

/* not very DRY wrt parse_shares.c */
void process_file(char *filename, int permutation, int64_t unique[3], uint64_t *H[3], struct share_t *Pre[3])
{
	struct share_t *SHARES = NULL;
	int64_t N = 0;
	int fd = -1;
	mmap_shares(filename, &SHARES, &N, &fd);
	int64_t done = 0;
	
	#pragma omp parallel
	{
		struct block_context_t *ctx = init_block_context();

		#pragma omp for
		for (int64_t i = 0; i < N; i++) {
			uint32_t block[20], hash[8];
			assemble_block(block, &SHARES[i], ctx);
			hash_block(hash, block);
			assert(hash[7] == 0);

			if (tid() == 0 && (done & 0xffff) == 0) {
				printf("%s : [%ld, %ld, %ld] / [%ld, %ld, %ld] / %ld / %ld\r", filename, 
					matches[0], matches[1], matches[2], 
					unique[0], unique[1], unique[2], 
					done, N);
				fflush(stdout);
			}
			#pragma omp atomic update	
			done++;
			int kind = SHARES[i].kind;
			char *h = (char *) hash;
			uint64_t *hh = (uint64_t *) (h + 19);

			/* binary search */
			int pkind = P[permutation][kind];
			int64_t idx = binary_search(H[pkind], 0, unique[pkind], *hh);
			if (idx < 0)
				continue;
			Pre[pkind][idx] = SHARES[i];
			matches[pkind] += 1;
		}
		free(ctx);
	}
	printf("\n");
	munmap_shares(SHARES, N, fd);
}


int main(int argc, char **argv) 
{
	char *log_filename = NULL;
	struct option longopts[3] = {
		{"journal", required_argument, NULL, 'j'},
		{"permutation", required_argument, NULL, 'p'},
		{NULL, 0, NULL, 0}
	};

	int permutation = 0;
	char ch;
	while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
	    	switch (ch) {
		case 'p':
			permutation = atoi(optarg);
			break;
		case 'j':
			log_filename = optarg;
			break;
		default:
			printf("Unknown option\n");
			exit(EXIT_FAILURE);
		}
  	}
  	if (!log_filename)
  		errx(1, "missing required argument --journal=...");
  	if (argc - optind == 0)
  		errx(1, "missing share filenames");

	uint64_t *H[3], *U[3];
	struct share_t *Pre[3];

	/* load solutions */
	int64_t total_solutions = load_solutions(log_filename, H);
	assert(total_solutions > 0);

	/* sort and uniquify */
	int64_t unique[3];
	for (int i = 0; i < 3; i++) {
		U[i] =  malloc(total_solutions * sizeof(*U[i]));
		sort_list_sequential(H[i], 0, total_solutions);
		int64_t k = 0;
		U[i][0] = H[i][0];
		for (int64_t j = 1; j < total_solutions; j++)
			if (H[i][j] != U[i][k])
				U[i][++k] = H[i][j];
		unique[i] = k + 1;
		printf("H[%d] : %ld unique values\n", i, k + 1);
	}

	/* find preimages of (unique) hashes */
	printf("Finding preimages...\n");
	for (int i = 0; i < 3; i++) {
	 	Pre[i] = malloc(unique[i] * sizeof(*Pre[i]));
		for (int64_t j = 0; j < unique[i]; j++)
			Pre[i][j].kind = -1;
	}
	while (optind < argc)
                process_file(argv[optind++], permutation, unique, U, Pre);

        /* check # of colliding bits */
        for (int k = 0; k < 3; k++)
        	free(H[k]);
        load_solutions(log_filename, H);


        struct block_context_t *ctx = init_block_context();
        int best = 0;
        struct share_t best_shares[3];
        for (int64_t i = 0; i < total_solutions; i++) {
        	uint32_t hashes[3][8];
        	int64_t idx[3];
        	for (int k = 0; k < 3; k++) {
			uint32_t block[20];

			idx[k] = binary_search(U[k], 0, unique[k], H[k][i]);
			assemble_block(block, &Pre[k][idx[k]], ctx);
			hash_block(hashes[k], block);
		}
		uint32_t full_hash[8];
		for (int j = 0; j < 8; j++)
			full_hash[j] = hashes[0][j] ^ hashes[1][j] ^ hashes[2][j];
		
		for (int j = 0; j < 8; j++)
			printf("%08x ", __builtin_bswap32(full_hash[j]));
		printf("\n");

		int score = __builtin_ctz(__builtin_bswap32(full_hash[4]));
		if (full_hash[6] == 0 && score > best) {
			best = score;
			for (int k = 0; k < 3; k++)
				best_shares[k] = Pre[k][idx[k]];
			printf("3SUM on %d bits\n", 96 + best);
			for (int j = 0; j < 8; j++)
				printf("%08x ", __builtin_bswap32(full_hash[j]));
			printf("\n");
		}
        }



        /* print winner */
	printf("best: 3SUM on %d bits\n", 96 + best);
        for (int k = 0; k < 3; k++) {
        	printf("Share(\"%08x\", \"%08x\", \"%08x\", kind=%d, D=%d, extranonce1=\"%08x\")\n", 
			best_shares[k].extraNonce2, best_shares[k].nonce, best_shares[k].ntime, 
        		best_shares[k].kind, best_shares[k].difficulty, best_shares[k].extraNonce1);
        }
}