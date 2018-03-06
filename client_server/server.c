#define DEFAULT_FILENAME "journal.log"
#define DEFAULT_ADDRESS "tcp://*:5555"

#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <getopt.h>
#include <sys/time.h>
#include <string.h>
#include <zmq.h>

#include "server.h"

enum state_t { PENDING, RUNNING, DONE };
struct task_t {
	int id;
	enum state_t state;
	double start_time;
	struct task_t *prev, *next;
};



struct option longopts[4] = {
	{"N", required_argument, NULL, 'N'},
	{"server-address", required_argument, NULL, 'a'},
	{"journal-file", required_argument, NULL, 'j'},
	{NULL, 0, NULL, 0}
};

int N = -1;
char *server_address = NULL;
char *journal_filename = NULL;


int N;
struct task_t *tasks;
int n_tasks[3];
struct task_t *first[3];
struct task_t *last[3];

int task_timeout = 3600;

char *journal_filename;
FILE *journal_file;
double total_time;

void set_task_state(int i, enum state_t new_state)
{
	struct task_t *t = &tasks[i];

	if (t->prev != NULL)
		t->prev->next = t->next;
	else
		first[t->state] = t->next;

	if (t->next != NULL)
		t->next->prev = t->prev;
	else
		last[t->state] = t->prev;
	n_tasks[t->state]--;

	t->state = new_state;
	t->prev = last[new_state];
	last[new_state] = t;
	t->next = NULL;
	n_tasks[new_state]++;
}

double wtime()
{
	struct timeval ts;
	gettimeofday(&ts, NULL);
	return (double)ts.tv_sec + ts.tv_usec / 1E6;
}

int main(int argc, char **argv)
{
	server_address = strdup(DEFAULT_ADDRESS);
	journal_filename = strdup(DEFAULT_FILENAME);

	char ch;
	while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
		switch (ch) {
		case 'N':
			N = atoi(optarg);
			break;
		case 'a':
			server_address = optarg;
			break;
		case 'j':
			journal_filename = optarg;
			break;
		default:
			errx(1, "Unknown option\n");
		}
	}
	if (N < 0)
		errx(1, "missing argument --N");

	/* prepare tasks */
	tasks = malloc(N * sizeof(struct task_t));
	if (tasks == NULL)
		err(1, "failed to allocate all tasks");
	for (int i = 0; i < N; i++) {
		tasks[i].id = i;
		tasks[i].state = PENDING;
	}
	tasks[0].prev = NULL;
	for (int i = 1; i < N; i++)
		tasks[i].prev = &tasks[i - 1];
	for (int i = 0; i < N - 1; i++)
		tasks[i].next = &tasks[i + 1];
	tasks[N - 1].next = NULL;
	first[PENDING] = &tasks[0];
	last[PENDING] = &tasks[N - 1];
	n_tasks[PENDING] = N;
	first[RUNNING] = NULL;
	last[RUNNING] = NULL;
	first[DONE] = NULL;
	last[DONE] = NULL;

	/* read journal */
	journal_file = fopen(journal_filename, "r");
	if (journal_file != NULL) {
		while (1) {
			struct record_t record;
			size_t x =
			    fread(&record, sizeof(struct record_t), 1,
				  journal_file);
			if (x != 1) {
				if (feof(journal_file))
					break;
				else
					err(1, "reading journal file");
			}
			set_task_state(record.id, DONE);
			total_time += record.duration;
			fseek(journal_file, record.size, SEEK_CUR);
		}
		fclose(journal_file);
	}

	journal_file = fopen(journal_filename, "a");
	if (journal_file == NULL)
		err(1, "Impossible to open journal file %s for append\n",
		    journal_filename);

	/* initialize networking */
	void *context = zmq_ctx_new();
	void *socket = zmq_socket(context, ZMQ_REP);
	if (zmq_bind(socket, server_address) != 0)
		errx(1, "zmq_bind : %s\n", zmq_strerror(errno));

	while (1) {		
		/* reschedule expired tasks */
		double threshold = wtime() - task_timeout;
		while (first[RUNNING] != NULL
		       && first[RUNNING]->start_time < threshold)
			set_task_state(first[RUNNING]->id, PENDING);

		zmq_msg_t zmsg;
		zmq_msg_init(&zmsg);
		int recvd_bytes = zmq_msg_recv(&zmsg, socket, 0);
		printf("Got %d bytes\n", recvd_bytes);
		if (recvd_bytes < 0)
			errx(1, "zmq_msg_recv %s", zmq_strerror(errno));
		struct message_t *msg = zmq_msg_data(&zmsg);

		int i;
		switch (msg->id) {
		case -1:
			/* task request */
			if (first[PENDING] == NULL) {
				i = -1;
			} else {
				i = first[PENDING]->id;
				set_task_state(i, RUNNING);
				tasks[i].start_time = wtime();
			}
			char *hostname = msg->data;
			printf("Sending task %d to %s\n", i, hostname);
			if (zmq_send(socket, &i, sizeof(int), 0) < 0)
				errx(1, "zmq_send : %s", zmq_strerror(errno));
			break;
		
		default:
			/* task result */
			printf("collecting task %d\n", msg->id);
			set_task_state(msg->id, DONE);
			struct record_t record;
			record.id = msg->id;
			record.start_time = tasks[msg->id].start_time;
			record.duration = wtime() - record.start_time;
			total_time += record.duration;
			record.size = recvd_bytes;
			fwrite(&record, sizeof(struct record_t), 1, journal_file);
			fwrite(msg, 1, recvd_bytes, journal_file);
			fflush(journal_file);
			zmq_send(socket, NULL, 0, 0);
			break;
		}
		zmq_msg_close(&zmsg);

		printf("task %d/%d, %d active, %.0f CPU.s, ", n_tasks[DONE], N, n_tasks[RUNNING], total_time);
	}
}