#include <stdio.h>
#include <stdlib.h>
#include <err.h>

#include "../membership.h"
#include "../util.h"
#include "tasks.h"
#include "../tooling/index.h"


void init_quad_context(struct quad_context *ctx, int k, int l, char **filenames)
{
	ctx->k = k;
	if (l < 0)             /* educated guess */
		l = k / 2;
	ctx->l = l;
	/* load hashes & index */
	for (int i = 0; i < 3; i++) {
		ctx->F[i] = fopen(filenames[i], "r");
		assert(ctx->F[i]);
		ctx->Idx[i] = load_index(filenames[i], k);
	}
}

static void load_hashes(int i, FILE *F, int64_t lo, int64_t hi, uint64_t *H)
{
	int rc = fseek(F, lo * sizeof(*H), SEEK_SET);
	assert(rc == 0);

	size_t check = fread(H, sizeof(*H), hi - lo, F);
	if ((int64_t) check != (hi - lo)) {
		printf("ERROR: i=%d, wanted %ld bytes [%ld:%ld], got %ld\n", i, hi - lo, lo, hi, check);
		assert((int64_t) check == hi - lo);
	}		
	printf("\t%c[%ld:%ld] (%.1f Mbyte)\n", 'A' + i, lo, hi, (hi - lo) / 131072.0);
}

struct result_t {
	int capacity;
	int size;
	uint64_t *data;
};

void solutions_append(struct result_t *result, uint64_t x, uint64_t y)
{
	int64_t i = result->size++;
	if (i >= result->capacity)
		errx(1, "TOO MANY SOLUTIONS\n");
	result->data[2 * i] = x;
	result->data[2 * i + 1] = y;
}


void do_task_quad(struct quad_context *ctx, int id, int *result_size, void **result_payload)
{
	int l = ctx->l;
	int k = ctx->k;
	int64_t shift = 1 << (k - l);
	int u = id % (1 << l);
	int v = id / (1 << l);
	printf("task (%d, %d): \n", u, v);

	struct result_t result;
	result.data = malloc(2 * 1024 * sizeof(uint64_t));
	result.capacity = 1024;
	result.size = 0;

	double start = wtime();

	/* prepare adapted indexes */
	int idx[3] = {v, u ^ v, u};
	uint64_t I[3][shift + 1];
	int64_t lo[3];
	int64_t hi[3];
	for (int i = 0; i < 3; i++) {
		lo[i] = ctx->Idx[i][idx[i] * shift];
		hi[i] = ctx->Idx[i][(idx[i] + 1) * shift];
		for (int j = 0; j < shift + 1; j++)
			I[i][j] = ctx->Idx[i][idx[i] * shift + j] - lo[i];
	}
	uint64_t *U = I[0];
	uint64_t *V = I[1];
	uint64_t *W = I[2];

	/* load hashes from disk */
	uint64_t A[hi[0] - lo[0]];
	uint64_t B[hi[1] - lo[1]];
	uint64_t C[hi[2] - lo[2]];
	load_hashes(0, ctx->F[0], lo[0], hi[0], A);
	load_hashes(1, ctx->F[1], lo[1], hi[1], B);
	load_hashes(2, ctx->F[2], lo[2], hi[2], C);

	printf("\tData load time: %.1f s\n", wtime() - start);

	uint64_t timer = rdtsc();

	/* Q3: loop on i*/
	int64_t n_pairs = 0;
	for (int64_t i = 0; i < shift; i++) {
		/* Q4: Initialize a static perfect hash table with entries of C^[i] */
		struct cuckoo_hash_t *H = init_cuckoo_hash(C, W[i], W[i + 1]);

		/* Q5: loop on j */
		for (int64_t j = 0; j < shift; j++) {

			/* Q6: check all pairs in A^[j] x B[i ^ j] */			
			int64_t A_lo = U[j];
			int64_t A_hi = U[j + 1];
			int64_t B_lo = V[i ^ j];
			int64_t B_hi = V[(i ^ j) + 1];

			n_pairs += (B_hi - B_lo) * (A_hi - A_lo);

			for (int64_t x = A_lo; x < A_hi; x++)
				for (int64_t y = B_lo; y < B_hi; y++)
					if (cuckoo_lookup(H, A[x] ^ B[y]))
						solutions_append(&result, A[x], B[y]);
		}
		free_cuckoo_hash(H);
	}
	printf("\t --> %.1fs (%.1f cycles / pair) %ld pairs, %d solutions\n",
		wtime() - start, (1.0 * (rdtsc() - timer)) / n_pairs, n_pairs, result.size);

	*result_size = 2 * sizeof(uint64_t) * result.size;
	*result_payload = result.data;
}