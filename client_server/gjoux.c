#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <err.h>
#include <math.h>

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
	argc -= optind;
	argv += optind;

	if (argc < 3) {
		printf("USAGE: %s [--join-width=WIDTH]  [--task-width=WIDTH] A.hash B.hash C.hash\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	struct gjoux_global_context ctx;
	init_gjoux_global_context(&ctx, join_width, task_width, argv);
	
	struct task_result_t result;
	result.n = 0;

	printf("|A| = %ld  |B| = %ld  |C| = %ld\n", ctx.N[0], ctx.N[1], ctx.N[2]);
	printf("using join-width=%d\n", ctx.k);
	printf("#slices = %ld, #tasks = %ld (%d slice / task)\n", ctx.n_slices, ctx.n_tasks, ctx.slices_per_task);

	printf("Go\n");
	double start_wtime = wtime();
	
	if (nworkers == 0)
		nworkers = 1;

	int task_done = 0;

	printf("Running %d workers concurrently (on %d places)\n", nworkers, num_places());
	#pragma omp parallel num_threads(nworkers) proc_bind(spread) 
	{
		struct gjoux_worker_context wctx;
		init_gjoux_worker_context(&wctx, &ctx);
		printf("worker %d/%d initialized (on place %d)\n", tid(), num_threads(), place_num());

		#pragma omp for schedule(dynamic, 1)
		for (int i = 0; i < ctx.n_tasks; i++) {
			result.id = i;
			result.work = 0;
			result.duration = 0;
			printf("task %d starting on worker %d\n", i, tid());
			do_task_gjoux(&wctx, &result);
			#pragma omp atomic update
			task_done++;

			double ETA = (wtime() - start_wtime) * ctx.n_slices / (ctx.slices_per_task * task_done);
			printf("total time : %g hours (%g days)\n", ETA / 3600, ETA / 3600 / 24);
		}
	}
	printf("\n%zu solutions found in %.1f s\n", result.n, wtime() - start_wtime);
	for (int64_t i = 0; i < result.n; i++)
		printf("%016" PRIx64 " ^ %016" PRIx64 " ... --> 0\n", result.result[2 * i], result.result[2 * i + 1]);
}

