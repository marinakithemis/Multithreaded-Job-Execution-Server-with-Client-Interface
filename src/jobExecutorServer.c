#include <stdio.h>
#include <sys/wait.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <netdb.h> 
#include <unistd.h> 
#include <stdlib.h> 
#include <ctype.h> 
#include <signal.h> 
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../include/structs.h"
#include "../include/header.h"


int job_count = 0; //current number of jobs in the buffer
int N = 1;  //concurrency
int ID = 0;  //job_ID
int cur_workers = 0; //current number of active workers 
int exit_flag = 0; 
int active_cont = 0; //current number of active controllers
pthread_mutex_t mutex;
pthread_mutex_t mutex_exit;
pthread_cond_t cond_buf_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_buf_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_exit = PTHREAD_COND_INITIALIZER;

int main(int argc, char* argv[])
{

    int port, buffer_size, thread_pool_size;

    if(argc != 4) error_exit("Wrong number of arguments\n");

    // check if user gives a valid input
    if(is_integer(argv[1])) port = atoi(argv[1]);
    else error_exit("Port should be an integer\n");

    if(is_integer(argv[2])) buffer_size = atoi(argv[2]);
    else error_exit("Buffer size should be an integer\n");

    if(buffer_size < 0) error_exit("Buffer size should be > 0\n");

    if(is_integer(argv[3])) thread_pool_size = atoi(argv[3]);
    else error_exit("Thread pool size should be an integer\n");
    
    struct job *job_buffer[buffer_size];

    int sock;
    struct sockaddr_in server;
    struct sockaddr *serverptr=(struct sockaddr *)&server;

    // Internet domain 
    server.sin_family = AF_INET; 
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port); 

    // Create socket 
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) error_exit("Error during socket\n");

    // Bind socket to address 
    if (bind(sock, serverptr, sizeof(server)) < 0) error_exit("Error during bind\n");

    // pass arguments to the mainthread through the struct pointer data_ptr
    pthread_t main_thread;
    struct mainth data;
    data.sock = sock;
    data.thread_pool_size = thread_pool_size;
    data.buffer = job_buffer;
    data.buffer_size = buffer_size;

    struct mainth* data_ptr = malloc(sizeof(struct mainth));
    if(data_ptr == NULL) error_exit("Error during malloc\n");

    *data_ptr = data;
    
    // create the mainthread
    if (pthread_create(&main_thread, NULL, mainthread, data_ptr) != 0) error_exit("Error during pthread_create\n");

    // mainthread keeps running
    pthread_join(main_thread, NULL); 

    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&mutex_exit);

    close(sock); 
    
    return 1;

}

void* mainthread(void* data_ptr)
{
    // initialize the mutexes
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&mutex_exit, NULL);

    struct mainth* data = (struct mainth *)data_ptr;

    int sock = data->sock;
    int thread_pool_size = data->thread_pool_size;

    socklen_t clientlen;
    int newsock;
    struct sockaddr_in client;
    struct sockaddr *clientptr=(struct sockaddr *)&client;

    // make the thread pool
    pthread_t th_pool[thread_pool_size];
    struct workth work;
    work.buffer = data->buffer;

    struct workth *work_ptr = malloc(sizeof(struct workth));
    if(data_ptr == NULL) error_exit("Error during malloc\n");
    *work_ptr = work;

    for(int i = 0; i < thread_pool_size; i++){
        if(pthread_create(&th_pool[i], NULL, workerthread, work_ptr) != 0) error_exit("Error during thread creation\n");
    }

    // listen for connections 
    if (listen(sock, 5) < 0) error_exit("Error during listen\n");

    pthread_t control_thread;
    
    while (1) { 

        clientlen = sizeof(client);

        // accept connection
        if ((newsock = accept(sock, clientptr, &clientlen)) < 0){
            pthread_mutex_lock(&mutex);

            if(exit_flag == 1){
                pthread_mutex_unlock(&mutex);
                break;
            }
        }

        char host[MAX_STR];
        if (getnameinfo((struct sockaddr *)&client, clientlen, host, sizeof(host), NULL, 0, NI_NAMEREQD) != 0) {
            error_exit("Error during getnameinfo\n");
        }

        // pass arguments to the controller thread through the struct pointer cont_ptr
        struct controllth cont;
        cont.buffer = data->buffer;
        cont.sock = newsock;
        cont.buffer_size = data->buffer_size;
        cont.socket = sock;

        struct controllth *cont_ptr = malloc(sizeof(struct controllth));
        if(cont_ptr == NULL) error_exit("Error during malloc\n");
        *cont_ptr = cont;

        // create a controller thread
        if (pthread_create(&control_thread, NULL, controller, cont_ptr) != 0) error_exit("Error during pthread_create\n"); 
        
        // detatch to prevent any leaks from the controller thread
        pthread_detach(control_thread); 

    }

    // wait for all controller threads to finish
    pthread_mutex_lock(&mutex_exit);
    while(active_cont != 0) pthread_cond_wait(&cond_exit, &mutex_exit);
    pthread_mutex_unlock(&mutex_exit);

    // wait for all worker threads to finish
    for (int i = 0; i < thread_pool_size; i++) {
        pthread_join(th_pool[i], NULL);
    }

    // deallocate resources and exit mainthread
    free(work_ptr);
    free(data_ptr);

    pthread_exit(NULL);
}

// function to insert a new job to the buffer
void insertJob(struct job *Job, struct job **buffer)
{
    buffer[job_count] = Job;
    job_count += 1; 

}

// function to send responses to the clients 
void send_message(char* message, int sock)
{
    int size = strlen(message);
    write(sock, &size, sizeof(int));
    write(sock, message, strlen(message));
}

void poll_command(int sock, struct job** buffer)
{
    pthread_mutex_lock(&mutex);

    // check first if the buffer doesn't contain any jobs
    if(job_count == 0){
        char* message = "Buffer is empty\n";
        send_message(message, sock);
        pthread_mutex_unlock(&mutex);
        return;
    }

    int size = 0;
    size += snprintf(NULL, 0, "------START POLL------\n");
    for(int i = 0; i < job_count; i++){
        size += snprintf(NULL, 0,  "<%s,%s>\n", buffer[i]->job_id, buffer[i]->print_job);
    }
    size += snprintf(NULL, 0, "------END POLL------\n");

    char* final_message = malloc((size+1)*sizeof(char));
    if(final_message == NULL) error_exit("Error during malloc\n");

    int cur = 0;
    cur += sprintf(final_message + cur, "------START POLL------\n");
    for(int i = 0; i < job_count; i++){
        cur += sprintf(final_message + cur, "<%s,%s>\n", buffer[i]->job_id, buffer[i]->print_job);
    }
    cur += sprintf(final_message + cur, "------END POLL------\n");

    final_message[size] = '\0';

    pthread_mutex_unlock(&mutex);

    send_message(final_message, sock);

    free(final_message);

}

// function to search the buffer for a specific job_id
struct job* search_buf(struct job** buffer, char* jobid)
{
    struct job* node = NULL;
    pthread_mutex_lock(&mutex);

    for(int i = 0; i < job_count; i++)
    {
        if(strcmp(buffer[i]->job_id, jobid) == 0){
            node = buffer[i];
            for(int j = i; j < job_count - 1; j++){
                buffer[j] = buffer[j+1];
            }
            job_count -= 1;
            pthread_cond_broadcast(&cond_buf_full);
            pthread_mutex_unlock(&mutex);

            return node;
        }
    }

    // return NULL if id doesn't exist in the buffer
    pthread_mutex_unlock(&mutex);
    return  NULL;
    
}

// function to change the concurrency
void setConcurrency(int newN, int sock)
{
    pthread_mutex_lock(&mutex);
    int old_N = N;
    N = newN;
    if(old_N < N) pthread_cond_broadcast(&cond_buf_empty);
    pthread_mutex_unlock(&mutex);

    char* message = malloc(MAX_STR*sizeof(char));
    sprintf(message, "CONCURRRENCY SET AT %d\n", newN);
    send_message(message, sock);
    free(message);

}

void stop_command(struct job** buffer, char* jobid, int sock)
{
    struct job *res = search_buf(buffer, jobid);
    if (res == NULL) {
        char* message = malloc(MAX_STR*sizeof(char));
        sprintf(message, "JOB %s NOTFOUND\n", jobid);
        send_message(message, sock);
        free(message);
    }
    else{
        char* message = malloc(MAX_STR*sizeof(char));
        sprintf(message, "JOB %s REMOVED\n", jobid);
        send_message(message, sock);
        send_message(message, res->socket);
        job_cleaning(res);
        free(message);
    }

}

// controller thread
void* controller(void* cont_ptr)
{
    pthread_mutex_lock(&mutex_exit);
    active_cont += 1;
    pthread_mutex_unlock(&mutex_exit);

    struct controllth* data = (struct controllth *)cont_ptr;
    int sock = data->sock;
    struct job** buffer = data->buffer;
    int buffer_size = data->buffer_size;

    // check if we have received an exit command 
    if(exit_flag == 1){
        free(data);
        pthread_mutex_lock(&mutex_exit);
        active_cont -= 1;
        if(active_cont == 0) pthread_cond_signal(&cond_exit);
        pthread_mutex_unlock(&mutex_exit);
        pthread_exit(NULL);
    }

    int size = 0;
    // read the size of the command
    read(sock, &size, sizeof(int));

    ssize_t bytes_read;
        
    int cur_size = 0;
    char command[size];

    //read safely the whole command
    while (cur_size < size) {
        bytes_read = read(sock, command + cur_size, size - cur_size);
        cur_size += bytes_read;
    }

    command[size] = '\0';

    // read the number of arguments of the command
    int num_of_args = 0;
    read(sock, &num_of_args, sizeof(int));

    char ** tokens = malloc((num_of_args + 1) * sizeof(char*));

    char *temp = strdup(command);
    tokens[0] = strtok(temp, "\x1F");
    
    for (int i = 1; i < num_of_args; i++) {
        tokens[i] = strdup(strtok(NULL, "\x1F"));
    }

    tokens[num_of_args] = NULL;

    // handle all possible commands
    if(strcmp(tokens[0], "issueJob") == 0){
        
        pthread_mutex_lock(&mutex);
        char* job_id = malloc(MAX_STR*sizeof(char));
        if(job_id == NULL) error_exit("Error during malloc\n");

        ID += 1;
        sprintf(job_id, "job_%d", ID);
        char *print_command = command_maker(tokens, num_of_args);

        struct job *JOB = malloc(sizeof(struct job));
        if(JOB == NULL) error_exit("Error during malloc\n");

        JOB->command = strdup(command);
        JOB->job_id = job_id;
        JOB->socket = sock;
        JOB->size = num_of_args;
        JOB->print_job = strdup(print_command);

        // check if the buffer is full, or if we have recieved an exit command
        while((job_count == buffer_size) && exit_flag == 0){
            pthread_cond_wait(&cond_buf_full, &mutex);
        }

        if(exit_flag == 1){
            char *message = "SERVER TERMINATED BEFORE EXECUTION\n";
            send_message(message, sock);
            job_cleaning(JOB);
            free(data);

            for (int i = 0; i < num_of_args + 1; i++) {
                free(tokens[i]);
            }

            free(print_command);
            free(tokens);

            pthread_mutex_lock(&mutex_exit);
            active_cont -= 1;
            if(active_cont == 0) pthread_cond_signal(&cond_exit);
            pthread_mutex_unlock(&mutex_exit);

            pthread_mutex_unlock(&mutex);
            pthread_exit(NULL);
        }

        // insert a new job to the buffer
        insertJob(JOB, buffer);
        
        // signal the buffer empty condition variable that the buffer is no longer empty
        pthread_cond_signal(&cond_buf_empty);

        pthread_mutex_unlock(&mutex);

        send_response(job_id, print_command, sock);
    }
    else if(strcmp(tokens[0], "setConcurrency") == 0){
        setConcurrency(atoi(tokens[1]), sock);
    }
    else if(strcmp(tokens[0], "poll") == 0){
        poll_command(sock, buffer);
    }
    else if(strcmp(tokens[0], "stop") == 0){
        stop_command(buffer, tokens[1], sock);
    }
    else if (strcmp(tokens[0], "exit") == 0){
        
        pthread_mutex_lock(&mutex);
        // set exit flag to 1
        exit_flag = 1;

        // cleanup the buffer
        for(int i = 0; i < job_count; i++){
            char* message = "SERVER TERMINATED BEFORE EXECUTION\n";
            send_message(message, buffer[i]->socket);
            job_cleaning(buffer[i]);
        }

        job_count = 0;

        char* message = "SERVER TERMINATED\n";
        send_message(message, sock);

        for (int i = 0; i < num_of_args + 1; i++) {
            free(tokens[i]);
        }

        free(tokens);

        // so that the mainthread gets unstuck from access function
        shutdown(data->socket, SHUT_RDWR);

        free(data);

        // wake up all worker threads
        pthread_cond_broadcast(&cond_buf_empty); 

        // wake up all controller threads
        pthread_cond_broadcast(&cond_buf_full); 

        pthread_mutex_lock(&mutex_exit);
        active_cont -= 1;

        // if all controllers finished, signal the exit condition variable
        if(active_cont == 0) pthread_cond_signal(&cond_exit);
        pthread_mutex_unlock(&mutex_exit);

        pthread_mutex_unlock(&mutex);

        pthread_exit(NULL);

    }
    else{
        char* message = "COMMAND DOES NOT EXIST\n";
        send_message(message, sock);
    }

    for (int i = 0; i < num_of_args + 1; i++) {
        free(tokens[i]);
    }

    free(tokens);
    free(data);

    pthread_mutex_lock(&mutex_exit);
    active_cont -= 1;
    if(active_cont == 0) pthread_cond_signal(&cond_exit);
    pthread_mutex_unlock(&mutex_exit);

    pthread_exit(NULL);
}

void send_response(char* job_id, char* print_command, int sock)
{
    int message_length = snprintf(NULL, 0, "JOB <%s,%s> SUBMITTED", job_id, print_command);
    char *message = malloc((message_length+1)*sizeof(char));
    sprintf(message, "JOB <%s,%s> SUBMITTED", job_id, print_command);
    write(sock, &message_length, sizeof(int));
    write(sock, message, message_length);
    free(message);
    free(print_command);

}

// function to get a job and execute it
void jobExecution(struct job *job)
{
    int size = job->size;
    char** tokens = malloc((size)*sizeof(char*));

    char* temp = strdup(job->command);
    strtok(temp, "\x1F");

    for (int i = 0; i < size-1; i++) {
        tokens[i] = strdup(strtok(NULL, "\x1F"));
    }
    
    tokens[size-1] = NULL;
    free(temp);

    // execute the actual command
    int pid = fork();

    // we are in the child process
    if(pid == 0){
        
        // make the pid.output file
        char filename[250];
        char start[100];
        sprintf(filename, "%d.output", getpid());
        int siz = sprintf(start, "-----%s output start-----\n", job->job_id);

        int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if(fd == -1) error_exit("Error during file creation\n");

        write(fd, start, siz);

        dup2(fd, STDOUT_FILENO);

        close(fd);

        if(execvp(tokens[0], tokens) == -1){
            if(execv(tokens[0], tokens) == -1) error_exit("Error during execv\n");
        }
    }
    else{

        //wait for the child process to finish
        waitpid(pid, NULL, 0);
        
        char filename[250];
        char end[150];
        int siz = sprintf(end, "-----%s output end-----\n", job->job_id);

        sprintf(filename, "%d.output", pid);

        int fd = open(filename, O_WRONLY | O_APPEND);
        if(fd == -1) error_exit("Error during file opening\n");
        write(fd, end, siz);

        close(fd);

        // send file contents to the client (commander)
        int read_fd = open(filename, O_RDONLY);
        if(read_fd == -1) error_exit("Error during file opening\n");

        char buffer[MAX_STR];
        int current_bytes = 0;

        while(1){

            current_bytes = read(read_fd, buffer, MAX_STR);
            
            if(current_bytes == 0) break;

            if(current_bytes == -1 && errno == EINTR) continue;

            if(current_bytes == -1) error_exit("Error during read\n");

            write(job->socket, buffer, current_bytes);
        }
        
        shutdown(job->socket, SHUT_WR);

        if(unlink(filename) == -1) error_exit("Error during unlink\n");
        
        close(read_fd);  

        
    }
    for (int i = 0; i < size-1; i++) {
        free(tokens[i]);
    }
    free(tokens);
}

void* workerthread(void* args)
{
    struct workth *data = (struct workth *)args;
    while(1){
        struct job *new_job;

        pthread_mutex_lock(&mutex);

        // check if exit has been initiated
        if(exit_flag == 1){
            pthread_mutex_unlock(&mutex);
            break;
        }

        // wait until buffer has jobs, or until concurrency > cur_workers
        while((job_count == 0 || cur_workers == N) && exit_flag == 0) pthread_cond_wait(&cond_buf_empty, &mutex);

        if(exit_flag == 1){
            pthread_mutex_unlock(&mutex);
            break;
        }
        
        cur_workers += 1;

        // get a job from the buffer
        new_job = data->buffer[0]; 
        for(int i = 0; i < job_count - 1; i++){
            data->buffer[i] = data->buffer[i+1];
        }
        job_count -= 1;

        pthread_cond_signal(&cond_buf_full);
        pthread_mutex_unlock(&mutex);

        // execute the job
        jobExecution(new_job);

        pthread_mutex_lock(&mutex);
        
        cur_workers -= 1;
        job_cleaning(new_job);
        pthread_mutex_unlock(&mutex);

    }
    pthread_exit(NULL);
}

bool is_integer(char* value) 
{
    int i = 0;
    while(value[i] != '\0'){
        if(isdigit(value[i]) == 0){
            return false;
        }
        i++;
    }
    return true;
}

//make a string with the full command
char* command_maker(char** command, int size)
{
    char* full_command;
    int length = 0;
    for(int i = 0; i < size; i++) {
        length += strlen(command[i]);
    }
    length += size;
    
    full_command = malloc((length+1)*sizeof(char));

    if(full_command == NULL) error_exit("Error during malloc\n");

    full_command[0] = '\0'; 

    for(int i = 0; i < size ; i++) {
        strcat(full_command, command[i]);
        if (i < size - 1) {
            strcat(full_command, " "); //add space character if not the last string
        }
    }
    
    return full_command;
}

// deallocate memory of a struct JOB
void job_cleaning(struct job* JOB)
{
    close(JOB->socket);
    free(JOB->job_id);
    free(JOB->command);
    free(JOB->print_job);
    free(JOB);
}