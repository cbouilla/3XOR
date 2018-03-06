#include <inttypes.h>
#include <err.h>

#include "tasks.h"

void solutions_append(struct task_result_t *r, uint64_t x, uint64_t y)
{
	int64_t i;
	#pragma omp atomic capture
	i = r->n++;

	if (i >= TASK_RESULT_SIZE)
		errx(1, "TOO MANY SOLUTIONS\n");
	r->result[2 * i] = x;
	r->result[2 * i + 1] = y;
}
