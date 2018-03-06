#include <math.h>
#include <stdio.h>
#include <m4ri/m4ri.h>

#include "linalg.h"

void matmul_init(mzd_t *M, struct matmul_table_t* T)
{
	for (int64_t i = 0; i < 8; i++) {
		int64_t lo = i * 8;
		int64_t hi = (i+1) * 8;
		T->tables[i][0] = 0;
		uint64_t tmp = 0;
		for (int64_t j = 1; j < (1u << (hi - lo)); j++) {
			int64_t k =  __builtin_ctzll(j);
			tmp ^= M->rows[lo + k][0];
			T->tables[i][j ^ (j >> 1)] = tmp;
		}
	}
}


void matmul(uint64_t * const L, uint64_t *O, int64_t N, struct matmul_table_t * const T)
{
	for (int64_t i = 0; i < N; i++) {
		uint64_t r = 0;
		uint64_t x = L[i];
		//for (int64_t k = 0; k < 8; k++)
		//	r ^= T->tables[k][(x >> (8 * k)) & 0x00ff];
		r ^= T->tables[0][x & 0x00ff];
		r ^= T->tables[1][(x >> 8) & 0x00ff];
		r ^= T->tables[2][(x >> 16) & 0x00ff];
		r ^= T->tables[3][(x >> 24) & 0x00ff];
		r ^= T->tables[4][(x >> 32) & 0x00ff];
		r ^= T->tables[5][(x >> 40) & 0x00ff];
		r ^= T->tables[6][(x >> 48) & 0x00ff];
		r ^= T->tables[7][(x >> 56) & 0x00ff];
		O[i] = r;
	}
}

mzd_t * list_slice(uint64_t *L, int64_t lo, int64_t hi)
{
	assert(m4ri_radix == 64);

	mzd_t *A = mzd_init(hi - lo, 64);
	for (int64_t i = lo; i < hi; i++)
		mzd_row(A, i - lo)[0] = L[i];

	return A;
}

int basis_change_matrix(uint64_t *A, int64_t lo, int64_t hi, struct matmul_table_t *T, struct matmul_table_t *T_inv)
{
	mzd_t *C = list_slice(A, lo, hi);
	
	/* echelonize */
	mzd_t *E = mzd_transpose(NULL, C);

  	mzp_t* P = mzp_init(E->nrows);
  	mzp_t* Q = mzp_init(E->ncols);
	mzd_pluq(E, P, Q, 0);
	
	mzd_t *L = mzd_init(64, 64);
	for (int64_t i = 0; i < 64; i++)
		mzd_row(L, i)[0] = (mzd_row(E, i)[0] & ((1ll << i) - 1)) | (1ll << i);
	mzd_apply_p_left_trans(L, P);

	/*mzd_t *EE = mzd_init(64, E->ncols);
	for (int64_t i = 0; i < 64; i++)
		mzd_row(EE, i)[0] = (mzd_row(E, i)[0] & ~((1ll << i) - 1));
	mzd_apply_p_right_trans(O, Q);
	mzd_addmul_naive(O, LL, EE);
	mzd_print(O);*/
	mzd_t *M_inv = mzd_transpose(NULL, L);
	matmul_init(M_inv, T_inv);

	mzd_t *M = mzd_inv_m4ri(NULL, M_inv, 0);
	matmul_init(M, T);
	
	mzd_free(M);
	mzd_free(M_inv);
	mzd_free(L);
	mzd_free(E);
	mzd_free(C);
	mzp_free(P);
	mzp_free(Q);
	return 1;
}



void print_vector(uint64_t v, int n)
{
	uint64_t m = 0x8000000000000000ull;
	printf("[");
	for (int i = 0; i < n; i++) {
		printf("%s", (v & m) ? "1" : " ");
		m >>= 1;
	}
	printf("]");
}

void print_list(uint64_t *L, int64_t N, int n)
{
	for (int64_t i = 0; i < N; i++) {
		print_vector(L[i], n);
		printf("\n");
	}
}