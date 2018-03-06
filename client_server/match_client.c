#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <err.h>
#include <math.h>

#include <zmq.h>

#include "../membership.h"
#include "../tooling/hashes.h"
#include "../tooling/index.h"
#include "../util.h"
#include "client_server.h"
#include "tasks.h"	
 
void usage()
{

	printf("USAGE: match_client (--quadratic|--gjoux) [--prefix-width=WIDTH] [--task-width=WIDTH] [--join-width=WIDTH] [--server-address=tcp://HOST:PORT] A.hash B.hash C.hash\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	struct option longopts[7] = {
		{"prefix-width", required_argument, NULL, 'p'},
		{"task-width", required_argument, NULL, 't'},
		{"join-width", required_argument, NULL, 't'},
		{"server-address", required_argument, NULL, 'a'},
		{"quadratic", no_argument, NULL, 'q'},
		{"gjoux", no_argument, NULL, 'g'},
		{NULL, 0, NULL, 0}
	};

	int prefix_width = -1;
	int task_width = -1;
	int join_width = -1;
	algo_t algo = NONE;
	char *server_address = NULL;
	char ch;
	while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
	    	switch (ch) {
		case 'p':
			prefix_width = atoi(optarg);
			break;
		case 't':
			task_width = atoi(optarg);
			break;
		case 'a':
			server_address = optarg;
			break;
		case 'q':
			algo = QUADRATIC;
			break;
		case 'g':
			algo = GJOUX;
			break;
		default:
			printf("Unknown option\n");
			exit(EXIT_FAILURE);
		}
  	}
	
	if (algo == NONE)
  		usage();

	if (algo == QUADRATIC && prefix_width < 0)
		usage();

	if (algo == GJOUX && task_width < 0)
		usage();

	argc -= optind;
	argv += optind;

	if (argc < 3)
		usage();

	struct quad_context qctx;
	struct gjoux_global_context ggctx;
	struct gjoux_worker_context gwctx;  /* a single gjoux worker for now */

	if (algo == QUADRATIC) {
		printf("[quad] setup\n");
		init_quad_context(&qctx, prefix_width, task_width, argv);
	} else {
		printf("[gjoux] setup\n");
		init_gjoux_global_context(&ggctx, join_width, task_width, argv);
		init_gjoux_worker_context(&gwctx, &ggctx);
	}

	/* defaults */
	if (!server_address)
		server_address = strdup(MATCH_SERVER);

	/******** client mode *************/
	printf("Connecting to %s...\n", server_address);
	void *context = zmq_ctx_new();
	void *requester = zmq_socket(context, ZMQ_REQ);
	assert(context);
	assert(requester);
	if (0 != zmq_connect(requester, server_address))
		errx(1, "zmq_connect : %s", zmq_strerror(errno));
	
	struct task_request_t req;
	req.tag = TASK_REQUEST;
	if (gethostname(req.hostname, HOSTNAME_SIZE))
		err(1, "gethostname failed");

	while (1) {
		/* ask for work */
		zmq_send(requester, &req, sizeof(req), 0);

		/* get task id */
		struct task_result_t result;
		zmq_recv (requester, &result.id, sizeof(result.id), 0);
		if (result.id == GAME_OVER)
			break;

		printf("Doing task %ld\n", result.id);
		result.n = 0;
		result.work = 0;
		result.duration = 0;

		if (algo == QUADRATIC)
			do_task_quad(&qctx, &result);
		if (algo == GJOUX)
			do_task_gjoux(&gwctx, &result);


		/* send task result */
		zmq_send(requester, &result, sizeof(uint64_t) * 2 * result.n + 3 * sizeof(int64_t) + sizeof(double), 0);
		zmq_recv(requester, NULL, 0, 0);
	}
	zmq_close (requester);
	zmq_ctx_destroy (context);
	return 0;
}
