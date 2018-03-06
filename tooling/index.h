#include <inttypes.h>

void save_index(char *hash_filename, int64_t *U, int prefix_width);
int64_t * load_index(char *hash_filename, int k);
