#include <stdio.h>
#include <sys/types.h> /* sockets */
#include <sys/socket.h> /* sockets */
#include <netinet/in.h> /* internet sockets */
#include <unistd.h> /* read, write, close */
#include <netdb.h> /* gethostbyaddr */
#include <stdlib.h> /* exit */
#include <string.h> /* strlen */
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>
#include "../include/structs.h"
#include "../include/header.h"

bool is_integer(char* value);

int main(int argc, char* argv[])
{
    int port;
    char* server_name;

    server_name = argv[1];

    //checking if user provided a valid port num
    if(is_integer(argv[2])) port = atoi(argv[2]);
    else error_exit("Port should be an integer\n");

    char **command = malloc((argc-1) * sizeof(char*));

    if (command == NULL) error_exit("Error during malloc\n");

    for(int i = 3; i < argc; i++){
        command[i-3] = argv[i];
    }

    command[argc-2] = NULL;

    // Make a string with the full command
    char * com = command_maker(command, argc-3);

    int sock;
    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct hostent *rem;
   
    // Create socket 
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) error_exit("Error during socket\n");

    // Find server address
    if ((rem = gethostbyname(server_name)) == NULL) error_exit("Error during gethostbyname\n"); 

    // Internet domain 
    server.sin_family = AF_INET;

    memcpy(&server.sin_addr, rem->h_addr_list[0], rem->h_length);

    server.sin_port = htons(port); 

    // Initiate connection
    if (connect(sock, serverptr, sizeof(server)) < 0)  error_exit("Error during connect\n");
    
    int com_size = strlen(com) + 1;

    int num_of_args = argc - 3;

    // send the size of the command to the server
    if(write(sock, &com_size, sizeof(int)) < 0) error_exit("Error during write\n");

    // send the command to the server
    if(write(sock, com, com_size) < 0) error_exit("Error during write\n");

    // send the number of arguments that the command has
    if(write(sock, &num_of_args, sizeof(int)) < 0) error_exit("Error during write\n");
    
    ssize_t bytes_read;
    int size;

    //read the size of the response that we are expecting
    read(sock, &size, sizeof(int));

    int cur_size = 0;
    char buffer[size];

    //read safely the whole server response
    while (cur_size < size) {
        bytes_read = read(sock, buffer + cur_size, size - cur_size);
        cur_size += bytes_read;
    }

    buffer[size] = '\0';

    //print the response
    printf("%s\n\n", buffer);
    fflush(stdout);

    //if command is issueJob, we have to read the output of the job execution
    if(strcmp(argv[3], "issueJob") == 0){

        char buffer[MAX_STR];

        ssize_t bytes_read = 0;

        // read the output in chunks, because we don't know the size of it
        while(1){
            bytes_read = read(sock, buffer, MAX_STR);
            
            // server already closed the socket
            if(bytes_read == 0) break;

            // case of an interrupted system call
            if(bytes_read == -1 && errno == EINTR) continue;

            if(bytes_read == -1) error_exit("Error during read\n");

            write(STDOUT_FILENO, buffer, bytes_read);
        }
    }
    
    free(command);
    free(com);
    
}

// function to check if a string is an integer
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
            strcat(full_command, "\x1F"); //add unprintable character if not the last string
        }
    }

    return full_command;

}