#ifndef LINALG_H
#define LINALG_H
#include <inttypes.h>

struct matmul_table_t {
	uint64_t tables[8][256] __attribute__((aligned(64)));;
};

int basis_change_matrix(uint64_t *L, int64_t lo, int64_t hi, struct matmul_table_t *T, struct matmul_table_t *T_inv);
void matmul(uint64_t * const L, uint64_t *O, int64_t N, struct matmul_table_t * const T);

void print_vector(uint64_t v, int n);
void print_list(uint64_t *L, int64_t N, int n);

static inline uint64_t matvec(uint64_t x, struct matmul_table_t * const T)
{
	uint64_t r = 0;
	r ^= T->tables[0][x & 0x00ff];
	r ^= T->tables[1][(x >> 8) & 0x00ff];
	r ^= T->tables[2][(x >> 16) & 0x00ff];
	r ^= T->tables[3][(x >> 24) & 0x00ff];
	r ^= T->tables[4][(x >> 32) & 0x00ff];
	r ^= T->tables[5][(x >> 40) & 0x00ff];
	r ^= T->tables[6][(x >> 48) & 0x00ff];
	r ^= T->tables[7][(x >> 56) & 0x00ff];
	return r;
}

#endif