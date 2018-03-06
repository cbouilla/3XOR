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

#include "shares.h"
#include "sha256.h"


uint32_t PREFIXES[3][9] = {
	{0x464f4f2d, 0x20202020, 0x20202020, 0x20202020, 0x20436861, 
	 0x726c6573, 0x20426f75, 0x696c6c61, 0x67756574},
	{ 0x4241522d, 0x20202020, 0x20202020, 0x20202020, 0x20506965, 
	  0x7272652d, 0x416c6169, 0x6e20466f, 0x75717565 },
	{ 0x464f4f42, 0x41522d20, 0x20202020, 0x20202020, 0x20202020, 
	  0x436c6169, 0x72652044, 0x656c6170, 0x6c616365 }
};

unsigned char COINBASE1[58] = {
	0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0xff, 0xff, 0xff, 0xff, 0x20, 0x02, 0x08, 0x62, 0x06, 0x2f, 0x50, 
	0x32, 0x53, 0x48, 0x2f, 0x04, 0xb8, 0x86, 0x4e, 0x50, 0x08};

unsigned char COINBASE2[51] = {
	0x07, 0x2f, 0x73, 0x6c, 0x75, 0x73, 0x68, 0x2f, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x00, 0xf2, 0x05, 0x2a, 0x01, 0x00, 0x00, 0x00, 0x19, 0x76, 0xa9,
	0x14, 0xd2, 0x3f, 0xcd, 0xf8, 0x6f, 0x7e, 0x75, 0x6a, 0x64, 0xa7, 0xa9,
	0x68, 0x8e, 0xf9, 0x90, 0x33, 0x27, 0x04, 0x8e, 0xd9, 0x88, 0xac, 0x00,
	0x00, 0x00, 0x00};


void mmap_shares(const char *filename, struct share_t **L_, int64_t *N_, int *fd_)
{
	assert(sizeof(**L_) == 20);
	int fd = open(filename, O_RDONLY);
	if (fd == -1)
		err(1, "open %s", filename);

	struct stat buffer;
	if (fstat(fd, &buffer))
		err(1, "fstat");

	uint64_t N = buffer.st_size / sizeof(**L_);

	struct share_t *L =  mmap(NULL, buffer.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (L == MAP_FAILED)
		err(1, "mmap");

	*L_ = L;
	*N_ = N;
	*fd_ = fd;
}

void munmap_shares(struct share_t *L, int64_t N, int fd)
{
	munmap(L, N * sizeof(*L));
        close(fd);
}


struct block_context_t *init_block_context()
{
	struct block_context_t *ctx = malloc(sizeof(*ctx));
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 9; j++)
			ctx->prefixes[i][j] = __builtin_bswap32(PREFIXES[i][j]);

	for (int i = 0; i < 58; i++)
		ctx->coinbase[i] = COINBASE1[i];

	for (int i = 0; i < 51; i++)
		ctx->coinbase[58 + 8 + i] = COINBASE2[i];

	return ctx;
}

/* attention : il y a moyen que ceci ne fonctionne pas sur une architecture
   big-endian */

void assemble_block(uint32_t *out, struct share_t *share, struct block_context_t *ctx)
{
	assert(share->kind >= 0 && share->kind < 3);
	for (int i = 0; i < 9; i++)
		out[i] = ctx->prefixes[share->kind][i];

	uint32_t *cb_nonces = (uint32_t *) &ctx->coinbase[58];
	cb_nonces[0] = __builtin_bswap32(share->extraNonce1);
	cb_nonces[1] = __builtin_bswap32(share->extraNonce2);

	/*for (int i =0; i < 117; i++)
		printf("%02x", ctx->coinbase[i]);
	printf("\n");
	exit(1);*/

	unsigned char md_in[32];
	SHA256(ctx->coinbase, 117, md_in);
	SHA256(md_in, 32, (unsigned char *) &out[9]);

	//for (int i = 9; i < 17; i++)
	//	out[i] = __builtin_bswap32(out[i]);

	out[17] = share->ntime;
	out[18] = __builtin_bswap32(0xdeadbeef);
	out[19] = share->nonce;
}

void hash_block(uint32_t *hash, uint32_t *block)
{
	unsigned char md[32];
	SHA256((unsigned char *) block, 80, md);
	SHA256((unsigned char *) md, 32, (unsigned char *) hash);
}

void print_share(struct share_t *s)
{
	printf("Share(\"%08x\", \"%08x\", \"%08x\", kind=%d, D=%d, extranonce1=\"%08x\")\n", 
		s->extraNonce2, s->nonce, s->ntime, 
		s->kind, s->difficulty, s->extraNonce1);
}