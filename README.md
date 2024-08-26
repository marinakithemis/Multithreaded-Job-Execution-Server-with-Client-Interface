# Multithreaded Job Execution Server with Client Interface

This project is a multithreaded job execution server with a client interface. It consists of two main components: a server (jobExecutorServer) that manages job requests and executes them using a pool of worker threads, and a client (jobCommander) that sends commands to the server. The server is designed to handle job execution requests in a concurrent manner while ensuring proper synchronization and communication between threads. The job queue is managed using a buffer with mutexes and condition variables to coordinate job handling and resource access. The project also includes testing scripts to validate functionality and memory management using tools like Valgrind.

## How to compile and run

- Compile using the command `make`

- Execute the server using the command: `./bin/jobExecutorServer <port> <buffer_size> <thread_pool_size>`

- Execute a commander using the command: `./bin/jobCommander <server_name> <port> <command>`

- Run the test scripts with: `./tests/<test>.sh`

- Remove the executables with: `make clean`

- To have the server execute a program (for example progDelay.c) you should have compiled it first and if you run issueJob command, you should put the executable of the program into the tests folder (e.g ./bin/jobCommander localhost 18122 issueJob ./tests/progDelay 1).

- I have included the source of progDelay on the src folder, so to run my tests just compile it and put the executable on the tests folder.

## Files and Folders

- bin/ : contains the executables (currently is empty, if you run the make command it will also include jobCommander and jobExecutorServer).

- build/ : contains the temporary build files (object files). Again, it is currently empty, run the make command and it will include (jobExecutorServer.o and jobCOmmander.o)

- include/: contains the header files header.h and structs.h .

- src/ : contains the source files jobCommander.c and jobExecutorServer.c

- tests/ : includes some bash script tests I made, that use valgrind as well. 

- Makefile

- README.md 

## jobCommander.c

- To execute a job commander you should have obviously executed the server beforehand, else the connection will fail.

- Checks if the user input is valid : checks number of arguments and if port is an actual integer.

- Creates the socket and finds the server ip address with the function gethostbyname.

- Initiates connection with the server.

- Writes the size of the command, the actual command (as a string) and the number of arguments to the socket.

- Waits until it reads safely the appropriate response from the server.

- Terminates only after receiving the server response.

## jobExecutorServer.c

- I use 2 mutexes and 3 condition variables to implement the synchronization of the server. Mutex is used by threads when they modify the job buffer, the concurrency and other shared variables, whilst mutex_exit gets occupied by the controller threads if they want to modify the active_cont variable.

- The buffer for the jobs is being implemented by using a static array of pointers to job structs. We can chose for the buffer to be statically allocated, as it has a specific size provided by the user, that won't change throughout the execution of the program.

### main function
- Again it checks if all the arguments provided by the user are valid.

- Creates the socket and binds it to address.

- Creates the mainthread and passes to it the appropriate arguments.

### mainthread

- The mainthread has a very important role for the server.

- Firstly, it makes the thread pool of the workerthreads and it initializes the two mutexes.

- Then, it listens for connections from clients (commanders).

- It accepts each connection inside a while loop, and for each client it creates a controller thread to handle the command. 

- I should note here, that if accept fails, it means that the server has shutdown its sock, so exit has been probably initiated and we should exit the mainthread.

- After the mainthread breaks its while loop, it should wait for all controller threads to finish (so that we don't have any leaks) and then it should wait for all the worker threads to finish.

- The waiting for the controller threads to finish is being achieved by using the condition variable exit and the global variable active_cont (active controllers). If we have no active controllers left, a signal is being sent to the condition variable stating that the mainthread should continue its execution. I use pthread_detach for the controllers so that after a controller thread terminates, all of its resources are being freed.

- The waiting for the worker threads is being achieved by using the pthread_join function.

### controller threads

- The controller threads job is to get and handle each command that the commander gives appropriately.

- It savely reads the command from the commander and then it takes some actions based on the actual command.

- When the controller recieves an issueJob command, it is important to check if the job buffer has enough space to insert the command. This is achieved by using the condition variable cond_buf_full, that makes the commander wait until the buffer is not full.

- When exit command is being received, we have to initiate exit. The controller does this by setting the value of the global variable exit_flag to 1. 

- The exit_flag indicates the request of a client to terminate the server, so all threads should regularly check if the value of the flag is set to 1.

- If the user provides the server with an invalid command, server just sends the message "COMMAND DOES NOT EXIST" to the commander.

### worker threads

- They are constantly active, waiting for the appropriate circumstances (concurrency, not empty buffer) to take a job (from issueJob command) and execute it.

- Again, the logic is similar to the controllers, we have the condition variable cond_buf_empty where each worker thread waits until the buffer has jobs or until concurrency > current workers (with current workers being the worker threads that currently execute jobs).

- As for the job execution, the child process uses dup2 to write the output of the job to a file and then the parent process waits for the child to terminate and writes the output to the socket, for the client.


## General Comments

- I use valgrind in the tests and I have 0 leaks.

- I have modified progDelay a bit to just print the $ (just to have a cleaner output).

- I delete the .output files that the worker threads make after I send the response to the client.

- I have put localhost as the server name on my tests.

- It is obvious that the server should have started execution before we send a client to him (that's why I've put sleep 4 after the execution of the server on my tests).

- It is REALLY important to check your tests for race conditions. I have added sleep 0.2 between each command to avoid them. If I hadn't put it, all commanders would run concurrently on a random order and then you might encounter issues (eg. if exit command run first while a commander tries to connect to the server "connection reset" error would come up).

