#include <inttypes.h>

extern char *EXT[3];

void mmap_hashes(const char *filename, uint64_t **L_, int64_t *N_, int *fd_, int read_write);
void munmap_hashes(uint64_t *L, int64_t N, int fd);
void save_hashes(char *block_filename, char *hash_dir, int kind, uint64_t *L, int64_t N);
