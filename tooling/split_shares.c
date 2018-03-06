#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <getopt.h>

#include "shares.h"

#define BUFFER_SIZE 1024


int main(int argc, char **argv)
{
	assert(sizeof(struct share_t) == 20);
	struct option longopts[3] = {
		{"nparts", required_argument, NULL, 'n'},
		{"target", required_argument, NULL, 't'},
		{NULL, 0, NULL, 0}
	};

	char ch;
	int nparts = -1;
	char *target = NULL;
	while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
	    	switch (ch) {
		case 'n':
			nparts = atoi(optarg);
			break;
		case 't':
			target = optarg;
			break;
		default:
			printf("Unknown option\n");
			exit(EXIT_FAILURE);
		}
  	}
	
	if (!target || nparts < 0 || optind >= argc) {
		printf("USAGE: %s --nparts=... --target=share_dir/share_file input_share1 [input_share2]...\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	char *output_name[nparts];
	FILE *output[nparts];
	struct share_t buffer[nparts][BUFFER_SIZE];
	int  buffer_size[nparts];

	for (int i = 0; i < nparts; i++) {
		asprintf(&output_name[i], "%s.%d", target, i);
		printf("+ output %d : %s\n", i, output_name[i]);
		output[i] = fopen(output_name[i], "w");
		if (output[i] == NULL)
			err(1, "fopen");
		buffer_size[i] = 0;
	}

	while (optind < argc) {
		char *filename = argv[optind++];
		printf("Processing %s\n", filename);
		struct share_t *SHARES = NULL;
		int64_t N = 0;
		int fd = -1;
		mmap_shares(filename, &SHARES, &N, &fd);
	
		for (int i = 0; i < N; i++) {
			int j = (SHARES[i].extraNonce2 ^ SHARES[i].nonce ^ SHARES[i].ntime) % nparts;
			buffer[j][buffer_size[j]] = SHARES[i];
			buffer_size[j] += 1;
			if (buffer_size[j] == BUFFER_SIZE) {
				size_t check = fwrite(buffer[j], 20, buffer_size[j], output[j]);
				if (check != (size_t) buffer_size[j])
					err(1, "fwrite");
				buffer_size[j] = 0;
			}
		}
		munmap_shares(SHARES, N, fd);
	}

	for (int i = 0; i < nparts; i++) {
		size_t check = fwrite(buffer[i], 20, buffer_size[i], output[i]);
		if (check != (size_t) buffer_size[i])
			err(1, "fwrite");
		fclose(output[i]);
	}

        exit(EXIT_SUCCESS);
}
