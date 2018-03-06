#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <err.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#include "../tooling/hashes.h"
#include "../util.h"
#include "tasks.h"

int main(int argc, char **argv) 
{
	struct option longopts[4] = {
		{"join-width", required_argument, NULL, 'j'},
		{"task-width", required_argument, NULL, 'l'},
		{"workers", required_argument, NULL, 'w'},
		{NULL, 0, NULL, 0}
	};

	int join_width = -1;
	int task_width = -1;
	int nworkers = num_places();
	char ch;
	while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
	    	switch (ch) {
		case 'j':
			join_width = atoi(optarg);
			break;
		case 'l':
			task_width = atoi(optarg);
			break;
		case 'w':
			nworkers = atoi(optarg);
			break;
		default:
			printf("Unknown option\n");
			exit(EXIT_FAILURE);
		}
  	}
	if (argc - optind < 3) {
		printf("USAGE: %s [--join-width=WIDTH]  [--task-width=WIDTH] A.hash B.hash C.hash\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	struct gjoux_global_context ctx;
	init_gjoux_global_context(&ctx, join_width, task_width, argv + optind);
	
	printf("|A| = %ld  |B| = %ld  |C| = %ld\n", ctx.N[0], ctx.N[1], ctx.N[2]);
	printf("using join-width=%d / task_width = %d\n", ctx.k, ctx.l);
	printf("#slices = %ld, #tasks = %ld (%d slice / task)\n", ctx.n_slices, ctx.n_tasks, ctx.slices_per_task);

	if (nworkers == 0)
		nworkers = 1;

	int task_done = 0;

	printf("Go\n");
	double start_wtime = wtime();
	int n_solutions = 0;
	uint64_t solutions[2048];

	printf("Running %d workers concurrently (on %d places)\n", nworkers, num_places());
	#pragma omp parallel num_threads(nworkers) proc_bind(spread) 
	{
		struct gjoux_worker_context wctx;
		init_gjoux_worker_context(&wctx, &ctx);
		printf("worker %d/%d initialized (on place %d)\n", tid(), num_threads(), place_num());

		#pragma omp for schedule(dynamic, 1)
		for (int i = 0; i < ctx.n_tasks; i++) {
			printf("task %d starting on worker %d\n", i, tid());
			void *result;
			int result_size;

			do_task_gjoux(&wctx, i, &result_size, &result);
	
			#pragma omp critical (solutions_update)
			{
				memcpy(solutions + 2 * n_solutions, result, result_size);
				n_solutions += result_size / (2 * sizeof(uint64_t));
				task_done++;
				double ETA = (wtime() - start_wtime) * ctx.n_slices / (ctx.slices_per_task * task_done);
				printf("total time : %g hours (%g days)\n", ETA / 3600, ETA / 3600 / 24);
			}
			free(result);
			assert(n_solutions < 1024);
		}
	}

	printf("\n%d solutions found in %.1f s\n", n_solutions, wtime() - start_wtime);
	for (int i = 0; i < n_solutions; i++)
		printf("%016" PRIx64 " ^ %016" PRIx64 " ... --> 0\n", solutions[2 * i], solutions[2 * i + 1]);

	return (EXIT_SUCCESS);
}