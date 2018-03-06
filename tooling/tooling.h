#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <err.h>
#include <string.h>
#include <inttypes.h>

struct bucket_t {
	int64_t lo, hi;
	int prefix_width;
	uint64_t prefix;
};

struct bucket_list_t {
	int n;
	int capacity;
	struct bucket_t *B;
};


struct task_t {
	int64_t C_bucket_idx;
	int A_pw;
	int64_t A_bucket_idx_lo, A_bucket_idx_hi;
	int64_t pairs;
};

struct task_list_t {
	int n;
	int capacity;
	struct task_t *T;
};


double wtime();
uint64_t rdtsc();

void save_buckets(char *hash_filename, struct bucket_list_t *list);
struct bucket_list_t * load_buckets(char *hash_filename);
void add_bucket(struct bucket_list_t *list, int64_t lo, int64_t hi, int prefix_width, uint64_t prefix);

void add_task(struct task_list_t *tlist, int64_t c_idx, int A_pw, int64_t A_idx_lo, int64_t A_idx_hi, int64_t pairs);
void save_tasks(const char *filename, struct task_list_t *tlist);
struct task_list_t * load_tasks(char *filename);
