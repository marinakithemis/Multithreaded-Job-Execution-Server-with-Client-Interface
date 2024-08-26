#define error_exit(msg)    do { perror(msg); fflush(stdout); exit(0); } while (0)
#define MAX_STR 500 

#ifndef NI_NAMEREQD
#define NI_NAMEREQD 0x00000004
#endif

void* controller(void* cont_ptr);
void* mainthread(void* data_ptr);
bool is_integer(char* value);
void* workerthread(void* socket);
char* command_maker(char** command, int size);
void send_response(char* job_id, char* print_command, int sock);
void job_cleaning(struct job* JOB);
