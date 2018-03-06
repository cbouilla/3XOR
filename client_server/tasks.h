#include <stdio.h>

struct gjoux_global_context {
	char *C_filename;
	uint64_t * H[2]; /* read-only copy */
	int64_t N[3];
	int k, l;
	int64_t n_slices;
	int slice_height;
	int slices_per_task;
	int64_t n_tasks;
};


struct gjoux_worker_context {
	struct gjoux_global_context *global_ctx;
	int64_t pstride[2], tstride[2];
	uint64_t *scratch[2];
	uint64_t *HM[2];
	FILE *C_file;
	uint64_t *C;
};


struct quad_context {
	FILE *F[3];
	int64_t *Idx[3];
	int k, l;
};


struct result_t {
	int capacity;
	int size;
	uint64_t *data;
};

void solutions_init(struct result_t *r);
void solutions_append(struct result_t *r, uint64_t x, uint64_t y);
void solutions_finalize(struct result_t *r, int *result_size, void **result_payload);

void init_quad_context(struct quad_context *ctx, int k, int l, char **filenames);
void do_task_quad(struct quad_context *ctx, int id, int *result_size, void **result_payload);

void init_gjoux_global_context(struct gjoux_global_context *ctx, int k, int l, char **filenames);
void init_gjoux_worker_context(struct gjoux_worker_context *ctx, struct gjoux_global_context *glob_ctx);
void do_task_gjoux(struct gjoux_worker_context *wctx, int i, int *result_size, void **result_payload);

#define MAX(x, y) ((x < y) ? (y) : (x))
#define MIN(x, y) ((x < y) ? (x) : (y))
