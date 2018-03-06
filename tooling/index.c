#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <err.h>

int64_t * load_index(char *hash_filename, int k)
{
	char *filename_idx = malloc(strlen(hash_filename) + 5);
	strcpy(filename_idx, hash_filename);
	strcat(filename_idx, ".idx");

	FILE *file_idx = fopen(filename_idx, "r");
	if (file_idx == NULL)
		err(1, "fopen");

	int64_t *U = malloc((1 + (1ll << k)) * sizeof(int64_t));
	if (U == NULL)
		err(1, "malloc");
	size_t check = fread(U, sizeof(int64_t), 1 + (1ull << k), file_idx);
	if ((check != 1 + (1ull << k)) || ferror(file_idx))
		err(1, "fread on %s : read %zd, expected %ld", filename_idx, check, 1l << k);
	fclose(file_idx);
	free(filename_idx);
	return U;
}


void save_index(char *hash_filename, int64_t *U, int prefix_width)
{
	char *filename_idx = malloc(strlen(hash_filename) + 5);
	strcpy(filename_idx, hash_filename);
	strcat(filename_idx, ".idx");

	FILE *file_idx = fopen(filename_idx, "w");
	size_t check = fwrite(U, sizeof(*U), 1 + (1 << prefix_width), file_idx);
	fclose(file_idx);

	fprintf(stderr, "Wrote %zd bytes to %s\n", check*sizeof(*U), filename_idx);
	free(filename_idx);
}
