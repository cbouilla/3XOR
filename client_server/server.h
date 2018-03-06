struct record_t {
	int id;
	double start_time;
	double duration;
	int size;
	char payload[];
};

struct message_t {
	int id;
	int hostname_length;
	int result_size;
	char data[];
};