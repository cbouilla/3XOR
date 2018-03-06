#include <inttypes.h>
#include <stdlib.h>
#include <err.h>

#include "tasks.h"

void solutions_init(struct result_t *r)
{
	r->data = malloc(2 * 1024 * sizeof(uint64_t));
	r->capacity = 1024;
	r->size = 0;
}


void solutions_append(struct result_t *result, uint64_t x, uint64_t y)
{
	int64_t i = result->size++;
	if (i >= result->capacity)
		errx(1, "TOO MANY SOLUTIONS\n");
	result->data[2 * i] = x;
	result->data[2 * i + 1] = y;
}

void solutions_finalize(struct result_t *result, int *result_size, void **result_payload)
{
	*result_size = 2 * sizeof(uint64_t) * result->size;
	*result_payload = result->data;
}