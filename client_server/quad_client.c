#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <err.h>
#include <math.h>

#include <zmq.h>

#include "server.h"
#include "tasks.h"


int main(int argc, char **argv)
{
	/* parse command line */
	struct option longopts[4] = {
		{"server-address", required_argument, NULL, 'a'},
		/** work-specific options **/
		{"prefix-width", required_argument, NULL, 'p'},
		{"task-width", required_argument, NULL, 't'},
		{NULL, 0, NULL, 0}
	};
	char *server_address = NULL;
	int prefix_width = -1;
	int task_width = -1;
	char ch;
	while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
		switch (ch) {
		case 'a':
			server_address = optarg;
			break;
		case 'p':
			prefix_width = atoi(optarg);
			break;
		case 't':
			task_width = atoi(optarg);
			break;
		default:
			errx(1, "Unknown option\n");
		}
	}
	if (server_address == NULL)
		errx(1, "missing required option --server-address");

	if (prefix_width < 0)
		errx(1, "missing required option --prefix-width");

	if (argc - optind < 3)
		errx(1, "missing 3 filenames");

	/* initialize worker */
	struct quad_context ctx;
	init_quad_context(&ctx, prefix_width, task_width, argv + optind);
	int n_tasks = 1 << (2 * ctx.l);

	int64_t N[3] = {ctx.Idx[0][1 << ctx.k], ctx.Idx[1][1 << ctx.k], ctx.Idx[2][1 << ctx.k]};
	printf("|A| = %ld  |B| = %ld  |C| = %ld\n", N[0], N[1], N[2]);
	printf("using l=%d --> %d tasks, each requiring %ld bytes and about 2^%.1f pairs\n", 
		ctx.l, n_tasks, sizeof(uint64_t) * (N[0] + N[1] + N[2]) / (1l << ctx.l),
		log2(N[0]) + log2(N[1]) - 2*ctx.l);

	/* initialize networking */
	void *zcontext = zmq_ctx_new();
	void *zsocket = zmq_socket(zcontext, ZMQ_REQ);
	if (0 != zmq_connect(zsocket, server_address))
		errx(1, "zmq_connect : %s", zmq_strerror(errno));

	/* prepare task_request message */
	struct message_t *request_msg = malloc(sizeof(struct message_t) + 128);
	request_msg->id = -1;
	request_msg->result_size = 0;
	if (gethostname(request_msg->data, 128))
		err(1, "gethostname failed");
	char *hostname = request_msg->data;
	int hostname_length = strlen(hostname) + 1;


	/* main loop */
	while (1) {
		int id;

		/* send task request */
		zmq_send(zsocket, request_msg,
			 sizeof(struct message_t) + hostname_length, 0);
		zmq_recv(zsocket, &id, sizeof(int), 0);
		if (id < 0)
			break;
		printf("got id=%d\n", id);

		/* perform task */
		void *result;
		int result_size;
		do_task_quad(&ctx, id, &result_size, &result);

		/* send result */
		int response_size = sizeof(struct message_t) + hostname_length + result_size;
		struct message_t *response = malloc(response_size);
		response->id = id;
		response->hostname_length = hostname_length;
		response->result_size = result_size;
		memcpy(response->data, hostname, hostname_length);
		memcpy(response->data + hostname_length, result, result_size);
		zmq_send(zsocket, response, response_size, 0);
		zmq_recv(zsocket, NULL, 0, 0);
		free(response);
		free(result);
	}

	/* exit cleanly */
	printf("Bye.\n");
	zmq_close(zsocket);
	zmq_ctx_destroy(zcontext);
	return (EXIT_SUCCESS);
}