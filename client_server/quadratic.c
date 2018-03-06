#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <err.h>
#include <math.h>
#include <string.h>

#include "../util.h"
#include "tasks.h"


int main(int argc, char **argv) 
{
	struct option longopts[3] = {
		{"prefix-width", required_argument, NULL, 'p'},
		{"task-width", required_argument, NULL, 't'},
		{NULL, 0, NULL, 0}
	};

	int prefix_width = -1;
	int task_width = -1;
	char ch;
	while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
	    	switch (ch) {
		case 'p':
			prefix_width = atoi(optarg);
			break;
		case 't':
			task_width = atoi(optarg);
			break;
		default:
			printf("Unknown option\n");
			exit(EXIT_FAILURE);
		}
  	}
	if (prefix_width < 0 || argc < 3) {
		printf("USAGE: %s --prefix-width=WIDTH [--task-width=WIDTH] A.hash B.hash C.hash\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	
	/* initialize worker */
	struct quad_context ctx;
	init_quad_context(&ctx, prefix_width, task_width, argv + optind);
	int n_tasks = 1 << (2 * ctx.l);

	int64_t N[3] = {ctx.Idx[0][1 << ctx.k], ctx.Idx[1][1 << ctx.k], ctx.Idx[2][1 << ctx.k]};
	printf("|A| = %ld  |B| = %ld  |C| = %ld\n", N[0], N[1], N[2]);
	printf("using l=%d --> %d tasks, each requiring %ld bytes and about 2^%.1f pairs\n", 
		ctx.l, n_tasks, sizeof(uint64_t) * (N[0] + N[1] + N[2]) / (1l << ctx.l),
		log2(N[0]) + log2(N[1]) - 2*ctx.l);

	printf("Go\n");
	double start_wtime = wtime();
	int n_solutions = 0;
	uint64_t solutions[2048];


	for (int i = 0; i < n_tasks; i++) {
		void *result;
		int result_size;
		do_task_quad(&ctx, i, &result_size, &result);
		memcpy(solutions + 2 * n_solutions, result, result_size);
		free(result);
		n_solutions += result_size / (2 * sizeof(uint64_t));
		double ETA = (wtime() - start_wtime) * n_tasks / (i + 1);
		printf("ETA : %g hours (%g days)\n", ETA / 3600, ETA / 3600 / 24);
	}

	printf("\n%d solutions found in %.1f s\n", n_solutions, wtime() - start_wtime);
	for (int i = 0; i < n_solutions; i++)
		printf("%016" PRIx64 " ^ %016" PRIx64 " ... --> 0\n", solutions[2 * i], solutions[2 * i + 1]);

	return (EXIT_SUCCESS);
}