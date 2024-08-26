#include <pthread.h>


struct controllth{
    int sock;
    int buffer_size;
    struct job** buffer;
    int socket;
};

struct job{
    char* command;
    char* job_id;
    char* print_job;
    int socket;
    int size;
};

struct workth{
    struct job **buffer;
};

struct mainth{
    int sock;
    int thread_pool_size;
    int buffer_size;
    struct job **buffer;
    pthread_t main_thread;
};
