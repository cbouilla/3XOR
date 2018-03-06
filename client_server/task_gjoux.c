#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
#include <err.h>

#include "../util.h"
#include "../bucket_sort.h"
#include "../linalg.h"
#include "../membership.h"
#include "../tooling/hashes.h"
#include "tasks.h"

/* global strategy to deal with data volume :
   *) ONE SINGLE copy of the original hashes is kept in RAM for the WHOLE machine (regardless of #sockets, #cores, etc.)
      -> this is what the struct gjoux_global_ctx holds
   *) SEVERAL tasks are started (one per socket is an educated guess). Each task has its own scratch space, with space for A', B'.
      -> this is what the struct gjoux_task_ctx contains
   *) several threads may cooperate to deal with a single task.
*/

void init_gjoux_global_context(struct gjoux_global_context *ctx, int k, int l, char **filenames)
{
	int fd0, fd1;
	mmap_hashes(filenames[0], &ctx->H[0], &ctx->N[0], &fd0, 0);
	mmap_hashes(filenames[1], &ctx->H[1], &ctx->N[1], &fd1, 0);

	ctx->C_filename = filenames[2];

	struct stat buffer;
	if (stat(ctx->C_filename, &buffer))
		err(1, "fstat");

	ctx->N[2] = buffer.st_size / sizeof(uint64_t);

	if (k < 0)
		k = ceil(MAX(log2(ctx->N[0]), log2(ctx->N[1])));
	ctx->k = k;
	ctx->slice_height = 64 - ctx->k;
	ctx->n_slices = ceil(ctx->N[2] / ctx->slice_height);
	if (l < 0)
		l = ceil(log2(ctx->n_slices) / 2);
	ctx->l = l;
	ctx->n_tasks = 1 << (2 * ctx->l);
	ctx->slices_per_task = MAX(1, ceil(1.0 * ctx->n_slices / ctx->n_tasks));
}

void init_gjoux_worker_context(struct gjoux_worker_context *wctx, struct gjoux_global_context *gctx)
{
	wctx->global_ctx = gctx;
	/* alloc scratch space */
	for (int i = 0; i < 2; i++) {
		wctx->HM[i] = malloc(gctx->N[i] * sizeof(uint64_t));
		wctx->scratch[i] = malloc(gctx->N[i] * sizeof(uint64_t));
	}
	/* open file (in thread) and allocate space for (a portion of) C */
	wctx->C_file = fopen(gctx->C_filename, "r");
	assert(wctx->C_file);
	wctx->C = malloc(sizeof(uint64_t) * gctx->slices_per_task * gctx->slice_height);
}

void do_task_gjoux_slice(struct gjoux_worker_context *wctx, int64_t i, int64_t hashes_available, struct task_result_t *result)
{
	struct gjoux_global_context *ctx = wctx->global_ctx;
	struct task_result_t local_result;
	local_result.n = 0;

	double start = wtime();
	// uint64_t timer = rdtsc();

	/* G2 : Compute change of basis */
	struct matmul_table_t M, M_inv;
	int64_t C_lo = i * ctx->slice_height;
	int64_t C_hi = MIN(hashes_available, (i + 1) * ctx->slice_height);
	int64_t slice_height = C_hi - C_lo;

	#pragma omp critical(M4RI_is_not_thread_safe_for_god_s_sake)
	basis_change_matrix(wctx->C, C_lo, C_hi, &M, &M_inv);
	
	uint64_t CM[slice_height];
//	uint64_t otimer = rdtsc();

	/* G3 : matrix multiplication */
	matmul(ctx->H[0], wctx->HM[0], ctx->N[0], &M);
	matmul(ctx->H[1], wctx->HM[1], ctx->N[1], &M);
	matmul(wctx->C + C_lo, CM, slice_height, &M);

//	uint64_t timer = rdtsc();
//	printf("matmul : %.0f cycles / list element\n", 1.0 * (timer - otimer) / (ctx->N[0] + ctx->N[1] + C_hi - C_lo));

	/* M1 : sort A, B according to the first k bits */
	uint64_t *AS = radix256_sort(wctx->HM[0], wctx->scratch[0], ctx->N[0], ceil(ctx->k / 8.));
	uint64_t *BS = radix256_sort(wctx->HM[1], wctx->scratch[1], ctx->N[1], ceil(ctx->k / 8.));

//	printf("tri : %.0f cycles / list element\n", 1.0 * (rdtsc() - timer) / (ctx->N[0] + ctx->N[1]));

	struct cuckoo_hash_t *H = init_cuckoo_hash(CM, 0, slice_height);

//	timer = rdtsc();

	/* G4 : match */
	int64_t a = 0;
	int64_t b = 0;
	uint64_t mask = LEFT_MASK(ctx->k);

	while (a < ctx->N[0] && b < ctx->N[1]) {
		
		uint64_t prefix_a = AS[a] & mask;
		uint64_t prefix_b = BS[b] & mask;
		if (prefix_a < prefix_b) {
			a += 1;
			continue;
		}
		if (prefix_a > prefix_b) {
			b += 1;
			continue;
		}
		/* prefix_a == prefix_b */
		int64_t b_0 = b;
		while (1) {
			/* M4 : check pair */
			if (cuckoo_lookup(H, AS[a] ^ BS[b]))
				solutions_append(&local_result, AS[a], BS[b]);

			/* advance B */
			b += 1;
			if (b < ctx->N[1] && (BS[b] & mask) == prefix_b)
				continue;
			/* went too far in B : reset B, advance A*/
			a += 1;
			if (a < ctx->N[0] && (AS[a] & mask) == prefix_a) {
				b = b_0;
				continue;
			}
			/* went too far in A */
			break;
		}
	}
//	printf("match : %.0f cycles / list element\n", 1.0 * (rdtsc() - timer) / (ctx->N[0] + ctx->N[1]));
	free_cuckoo_hash(H);	
	// uint64_t match_timer = rdtsc();

	/* fix solutions */
	matmul(local_result.result, local_result.result, 2 * local_result.n, &M_inv);
	for (int64_t j = 0; j < local_result.n; j++)
		solutions_append(result, local_result.result[2 * j], local_result.result[2 * j + 1]);

	result->duration += wtime() - start;
	result->work += ctx->N[0] + ctx->N[1];
//	if ((i & 0xffff) == 0)
//	printf("tid: %d, task %ld: %.1fs (%.1f cycles / list item) , %ld solutions\n", tid(), i, 
//		wtime() - start, (1.0 * (rdtsc() - timer)) / (ctx->N[0] + ctx->N[1]), result->n);
}


void do_task_gjoux(struct gjoux_worker_context *wctx, struct task_result_t *result) {
	int64_t i = result->id;
	struct gjoux_global_context *ctx = wctx->global_ctx;

	printf("Task %ld:\n", i);
	
	int64_t slice_lo = i * ctx->slices_per_task;
	int64_t slice_hi = MIN(ctx->n_slices, (i + 1) * ctx->slices_per_task);

	if (slice_lo >= ctx->n_slices)
		return;
	
	printf("\tSlices [%ld:%ld]\n", slice_lo, slice_hi);

	/* load required portion of C */
	double io_start = wtime();
	int64_t volume = ctx->slice_height * ctx->slices_per_task;
	int rc = fseek(wctx->C_file, sizeof(uint64_t) * slice_lo * ctx->slice_height, SEEK_SET);
	assert(rc == 0);
	int64_t hashes_available = fread(wctx->C, sizeof(uint64_t), volume, wctx->C_file);
	printf("\tData load time: %.1f s (%.1f Mbyte)\n", wtime() - io_start, volume / 131072.0);

	/* process the slices of C */
	uint64_t timer = rdtsc();
	for (int64_t j = 0; j < slice_hi - slice_lo; j++)
		do_task_gjoux_slice(wctx, j, hashes_available, result);

	printf("\t--> %.1fs (%.1f cycles / list item), %ld solutions\n",
		result->duration, 
		(1.0 * (rdtsc() - timer)) / (ctx->N[0] + ctx->N[1]) / ctx->slices_per_task, 
		result->n);
}