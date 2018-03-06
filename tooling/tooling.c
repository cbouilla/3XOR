#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <err.h>
#include <string.h>
#include <sys/time.h>

#include "tooling.h"



void save_buckets(char *hash_filename, struct bucket_list_t *list)
{
	char *filename_idx = malloc(strlen(hash_filename) + 9);
	strcpy(filename_idx, hash_filename);
	strcat(filename_idx, ".buckets");

	FILE *file_idx = fopen(filename_idx, "w");
	size_t check = fwrite(list->B, sizeof(struct bucket_t), list->n, file_idx);
	fclose(file_idx);

	fprintf(stderr, "Wrote %zd bytes to %s\n", check*sizeof(*list->B), filename_idx);
	free(filename_idx);
}



struct bucket_list_t * load_buckets(char *hash_filename)
{
	struct bucket_list_t *blist = malloc(sizeof(*blist));

	char *bucket_filename = malloc(strlen(hash_filename) + 9);
	strcpy(bucket_filename, hash_filename);
	strcat(bucket_filename, ".buckets");

	struct stat buffer;
	if (stat(bucket_filename, &buffer))
		err(1, "stat (%s)", bucket_filename);

	blist->n = buffer.st_size / sizeof(struct bucket_t);
	blist->capacity = blist->n;
	blist->B = malloc(sizeof(struct bucket_t) * blist->n);

	FILE *bucket_file = fopen(bucket_filename, "r");
	if (bucket_file == NULL)
		err(1, "fopen");

	size_t check = fread(blist->B, sizeof(struct bucket_t), blist->n, bucket_file);
	if ((check != (size_t) blist->n) || ferror(bucket_file))
		err(1, "fread on %s : read %zd, expected %d", bucket_filename, check, blist->n);
	fclose(bucket_file);
	free(bucket_filename);

	return blist;
}


void add_bucket(struct bucket_list_t *list, int64_t lo, int64_t hi, int prefix_width, uint64_t prefix)
{
	if (list->n == list->capacity) {
		list->capacity *= 2;
		list->B = realloc(list->B, list->capacity * sizeof(struct bucket_t));
	}
	int i = list->n++;
	list->B[i].lo = lo;
	list->B[i].hi = hi;
	list->B[i].prefix = prefix;
	list->B[i].prefix_width = prefix_width;
}

void add_task(struct task_list_t *tlist, int64_t c_idx, int A_pw, int64_t A_idx_lo, int64_t A_idx_hi, int64_t pairs)
{
	if (tlist->n == tlist->capacity) {
		tlist->capacity *= 2;
		tlist->T = realloc(tlist->T, tlist->capacity * sizeof(struct task_t));
	}
	int i = tlist->n++;
	tlist->T[i].C_bucket_idx = c_idx;
	tlist->T[i].A_pw = A_pw;
	tlist->T[i].A_bucket_idx_lo = A_idx_lo;
	tlist->T[i].A_bucket_idx_hi = A_idx_hi;
	tlist->T[i].pairs = pairs;
}

void save_tasks(const char *filename, struct task_list_t *tlist)
{
	FILE *tfile = fopen(filename, "w");
	if (tfile == NULL)
		err(1, "fopen, tasks.bin");
	fwrite(tlist->T, sizeof(struct task_t), tlist->n, tfile);
	fclose(tfile);
	fprintf(stderr, "wrote %ld bytes to %s\n", sizeof(struct task_t) * tlist->n, filename);
}

struct task_list_t * load_tasks(char *filename)
{
	struct task_list_t *tlist = malloc(sizeof(*tlist));

	struct stat buffer;
	if (stat(filename, &buffer))
		err(1, "stat (%s)", filename);

	tlist->n = buffer.st_size / sizeof(struct task_t);
	tlist->capacity = tlist->n;
	tlist->T = malloc(sizeof(struct task_t) * tlist->n);

	FILE *task_file = fopen(filename, "r");
	if (task_file == NULL)
		err(1, "fopen %s", filename);

	size_t check = fread(tlist->T, sizeof(struct task_t), tlist->n, task_file);
	if ((check != (size_t) tlist->n) || ferror(task_file))
		err(1, "fread on %s : read %zd, expected %d", filename, check, tlist->n);
	fclose(task_file);

	return tlist;
}