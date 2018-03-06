#define _XOPEN_SOURCE 500
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <err.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>

#include <zmq.h>

#include "client_server.h"
#include "tasks.h"
#include "../util.h"

#define MAX_TASK_RUNTIME 3600

/** task queue **/
typedef enum { PENDING, PROCESSING, DONE } TaskState;

/* task doubly-linked list */
struct tasklist_t {
	TaskState status;
	int64_t start_time;
	int next;
	int prev;
} ;

/* global variables */
struct tasklist_t *tasks;  /* list of ready tasks */
int64_t ready_task;        /* index of next ready task, or -1 */

int64_t tasks_done;        /* #completed tasks */
int64_t total_solutions = 0;
double total_time = 0.0;
int64_t total_work = 0;


/********************* ready tasks handling ******************************/
static void _remove_task(int id)
{
	assert(id >= 0);
	if (tasks[id].status != PENDING)
		return;
	if (tasks[id].prev < 0)
		ready_task = tasks[id].next;
	else
		tasks[tasks[id].prev].next = tasks[id].next;

	if (tasks[id].next >= 0)
		tasks[tasks[id].next].prev = tasks[id].prev;
}

/* mark a task as COMPLETED */
void task_done(int id)
{
	#pragma omp critical
	_remove_task(id);
	tasks[id].status = DONE;
	tasks_done++;
}

int remain_task() {
	return (ready_task >= 0);
}

/* return the id of the next ready task. Mark it as PROCESSING and remove it from the list of pending tasks. */
int next_ready_task()
{
	int id = ready_task;
	assert(tasks[id].status == PENDING);
	#pragma omp critical
	_remove_task(id);
	tasks[id].status = PROCESSING;
	tasks[id].start_time = time(NULL);
	return id;
}

void init_tasks(int n_tasks)
{
	tasks = malloc(n_tasks * sizeof(struct tasklist_t));
	for (int i = 0; i < n_tasks; i++) {
		tasks[i].status = PENDING;
		tasks[i].prev = i - 1;
		tasks[i].next = i + 1;
	}
	tasks[n_tasks - 1].next = -1;
	tasks_done = 0;
	ready_task = 0;
}

void _reinsert_task(int i)
{
	tasks[i].status = PENDING;
	tasks[i].prev = -1;
	tasks[i].next = ready_task;
	tasks[ready_task].prev = i;
	ready_task = i;
}

void restore_zombie_tasks(int n_tasks)
{
	int64_t now = time(NULL);
	for (int i = 0; i < n_tasks; i++) {
		if (tasks[i].status != PROCESSING)
			continue;
		if (now - tasks[i].start_time < MAX_TASK_RUNTIME)
			continue;
		printf("task %d has expired; marking as PENDING.\n", i);
		
		#pragma omp critical
		_reinsert_task(i);
	}
}

int processing_tasks(int n_tasks)
{
	int active = 0;
	for (int i = 0; i < n_tasks; i++)
		if (tasks[i].status == PROCESSING)
			active++;
	return active;
}


/********************** task result I/O ******************************/

/* parse the journal, clear completed tasks */
void parse_journal(const char *filename)
{
	FILE *log = fopen(filename, "r");
	if (!log) {
		printf("*) match journal file %s not found\n", filename);
		return;
	}

	printf("*) Reading match journal file %s\n", filename);

	while (!feof(log)) {
		int64_t task_id;
		int64_t solutions, work;
		double duration;
		uint64_t completion_time;
		
		/* read the task */
		if (1 != fread(&task_id, sizeof(int64_t), 1, log)) {
			if (feof(log))
				break;
			perror("reading task ID\n");
			exit(1);
		}
		if (1 != fread(&duration, sizeof(double), 1, log)) {
			perror("reading task duration\n");
			exit(1);
		};
		if (1 != fread(&completion_time, sizeof(uint64_t), 1, log)) {
			perror("reading completion time\n");
			exit(1);
		};
		if (1 != fread(&work, sizeof(int64_t), 1, log)) {
			perror("reading # items processed\n");
			exit(1);
		};
		if (1 != fread(&solutions, sizeof(int64_t), 1, log)) {
			perror("reading # solutions\n");
			exit(1);
		};
		if (-1 == fseek(log, 2 * sizeof(uint64_t) * solutions, SEEK_CUR)) {
			perror("skipping task A,B\n");
			exit(1);
		}

		task_done(task_id);
		total_solutions += solutions;
		total_time += duration;
		total_work += work;
	}
	fclose(log);
	printf("*) found %ld completed tasks in %s\n", tasks_done, filename);
}

void save_task(struct task_result_t * output)
{
	FILE *log = fopen(MATCH_LOG_FILE, "a");
	if (!log)
		err(1, "fopen in match master");
	fwrite(&output->id, sizeof(int64_t), 1, log);
	fwrite(&output->duration, sizeof(double), 1, log);
	int64_t completion = time(NULL);
	fwrite(&completion, sizeof(completion), 1, log);
	fwrite(&output->work, sizeof(int64_t), 1, log);
	fwrite(&output->n, sizeof(int64_t), 1, log);
	fwrite(output->result, sizeof(uint64_t), 2*output->n, log);
	fclose(log);
}

void usage()
{
	printf("USAGE: match_server (--quadratic|--gjoux) [--prefix-width=WIDTH] [--task-width=WIDTH] [--join-width=WIDTH] [--server-address=\"tcp://IP:PORT\"]\n");
	printf("\nWith --quadratic, --prefix-width is required.\n");
	printf("With --gjoux, --task-width is required.\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv) 
{
	struct option longopts[6] = {
		{"prefix-width", required_argument, NULL, 'p'},
		{"task-width", required_argument, NULL, 't'},
		{"server-address", required_argument, NULL, 'a'},
		{"quadratic", no_argument, NULL, 'q'},
		{"gjoux", no_argument, NULL, 'g'},
		{NULL, 0, NULL, 0}
	};

	int prefix_width = -1;
	int task_width = -1;
	char *server_address = NULL;
	char ch;
	algo_t algo = NONE;
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

	/* defaults */
	if (task_width < 0)
		task_width = prefix_width / 2;
	if (!server_address) 
		server_address = strdup(MATCH_SERVER);
	
	char *log_filename = strdup(MATCH_LOG_FILE);
	int64_t n_tasks = 1ll << (2 * task_width);
	init_tasks(n_tasks);
	parse_journal(log_filename);

	/*  Socket to talk to clients */
	void *context = zmq_ctx_new();
	void *responder = zmq_socket(context, ZMQ_REP);
	if (zmq_bind(responder, server_address) != 0)
		errx(1, "zmq_bind : %s\n", zmq_strerror(errno));	
	
	printf("Algorithm: %s\n", (algo == QUADRATIC) ? "quadratic" : "gjoux");
	printf("using T=%d --> %ld tasks\n", task_width, n_tasks);
	printf("server listening at %s\n", server_address);

	/* main loop. Answer task requests. Store tasks results */
	double start = -1.0;
	int end_notice = 0;
	total_work = 0;
	while (1) {
		union {
			struct task_request_t request;
			struct task_result_t output;
		} tmp;

		restore_zombie_tasks(n_tasks);

		if (processing_tasks(n_tasks) == 0 && !remain_task() && !end_notice) {
			end_notice = 1;
			printf("IT SEEMS THAT IT'S OVER!\n");
		}

		int recvd_bytes = zmq_recv(responder, &tmp, sizeof(tmp), 0);
		if (recvd_bytes < 0)
			errx(1, "zmq_recv %s", zmq_strerror(errno));
		if ((unsigned int) recvd_bytes > sizeof(tmp))
			errx(1, "task result too large, truncated");

		if (tmp.request.tag == TASK_REQUEST) {
	    		/** work request */
	    		int64_t task_id;
			if (remain_task())
				task_id = next_ready_task();
			else
				task_id = GAME_OVER;
			
			printf("\t sending task %ld to %s\n", task_id, tmp.request.hostname);
			if (zmq_send(responder, &task_id, sizeof(int64_t), 0) < 0)
				errx(1, "zmq_send : %s", zmq_strerror(errno));
			
			if (start < 0)
				start = wtime();
		} else { 
			/** task result */
			zmq_send(responder, NULL, 0, 0);  /* because of the REQ-REP pattern */
			if (tmp.output.n >= 0) {
				/* task did not fail */
				total_time += tmp.output.duration;
				total_work += tmp.output.work;   /* fixme */
				total_solutions += tmp.output.n;
				save_task(&tmp.output);
				task_done(tmp.output.id);
			}

			/* print progress report */
			printf("task %ld/%ld, %d active tasks, %.0f CPU.s, %ld solutions, ", 
				tasks_done, n_tasks, processing_tasks(n_tasks), total_time, total_solutions);
			if (algo == QUADRATIC)
				printf("%.0f M pairs/s\n", total_work / (wtime() - start) / 1e6);
			if (algo == GJOUX)
				printf("%.0f M item/s\n", total_work / (wtime() - start) / 1e6);
		}
	}
	zmq_close(responder);
	zmq_ctx_destroy(context);
	return 0;
}