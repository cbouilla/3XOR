#include <inttypes.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <err.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "hashes.h"

char *EXT[3] = {".foo", ".bar", ".foobar"};

void save_hashes(char *block_filename, char *hash_dir, int kind, uint64_t *L, int64_t N)
{
	char *block_basename = basename(block_filename);
	char *hash_filename = malloc(strlen(block_basename) + strlen(hash_dir) + 9);
	strcpy(hash_filename, hash_dir);
	strcat(hash_filename, "/");
	strcat(hash_filename, block_basename);
	strcat(hash_filename, EXT[kind]);

	FILE *hash_file = fopen(hash_filename, "w");
	if (!hash_file)
		err(1, "open %s for writing", hash_filename);

	size_t check = fwrite(L, sizeof(*L), N, hash_file);
	fclose(hash_file);

	fprintf(stderr, "Wrote %zd bytes to %s\n", check*sizeof(*L), hash_filename);
	free(hash_filename);
}


void mmap_hashes(const char *filename, uint64_t **L_, int64_t *N_, int *fd_, int read_write)
{
	int fd = open(filename, read_write ? O_RDWR : O_RDONLY);
	if (fd == -1)
		err(1, "open %s", filename);

	struct stat buffer;
	if (fstat(fd, &buffer))
		err(1, "fstat");

	uint64_t N = buffer.st_size / sizeof(uint64_t);

	uint64_t *L =  mmap(NULL, buffer.st_size, read_write ? (PROT_READ | PROT_WRITE) : PROT_READ, MAP_SHARED, fd, 0);
	if (L == MAP_FAILED)
		err(1, "mmap");

	/* safety check */
	if (!read_write)
		for (uint64_t i = 0; i < N - 1; i++)
			assert(L[i] <= L[i+1]);

	*L_ = L;
	*N_ = N;
	*fd_ = fd;
}

void munmap_hashes(uint64_t *L, int64_t N, int fd)
{
	munmap(L, N * sizeof(*L));
        close(fd);
}
