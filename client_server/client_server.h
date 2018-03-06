#define HOSTNAME_SIZE 128

typedef enum {QUADRATIC, GJOUX, NONE} algo_t;

struct task_request_t {
	int64_t tag;
	char hostname[HOSTNAME_SIZE];
};

#define GAME_OVER -1
#define TASK_REQUEST -2

const char* MATCH_SERVER = "tcp://127.0.0.1:5556";
const char* MATCH_LOG_FILE = "data/match_journal.log";
const char* SOLUTION_FILE = "solutions.bin";