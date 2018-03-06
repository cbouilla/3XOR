#ifndef BLOCKS_H
#define BLOCKS_H
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

struct share_t {
	int16_t kind, difficulty;
	uint32_t extraNonce2, extraNonce1;
	uint32_t nonce, ntime;
};

struct block_context_t {
	unsigned char coinbase[117];
	uint32_t prefixes[3][9];
};

void mmap_shares(const char *filename, struct share_t **L_, int64_t *N_, int *fd_);
void munmap_shares(struct share_t *L, int64_t N, int fd);
struct block_context_t *init_block_context();
void assemble_block(uint32_t *out, struct share_t *share, struct block_context_t *ctx);
void hash_block(uint32_t *hash, uint32_t *block);
void print_share(struct share_t *s);

#endif