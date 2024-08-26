valgrind --leak-check=full ./bin/jobExecutorServer 18122 10 5 &
sleep 4
./bin/jobCommander localhost 18122 issueJob ./tests/progDelay 1 &
sleep 0.2
./bin/jobCommander localhost 18122 issueJob ./tests/progDelay 1 &
sleep 0.2
./bin/jobCommander localhost 18122 issueJob ./tests/progDelay 1 &
sleep 0.2
./bin/jobCommander localhost 18122 setConcurrency 3 &
sleep 0.2
./bin/jobCommander localhost 18122 issueJob ./tests/progDelay 1 &
sleep 0.2
./bin/jobCommander localhost 18122 issueJob ./tests/progDelay 1 &
sleep 0.2
./bin/jobCommander localhost 18122 issueJob ./tests/progDelay 1 &
sleep 0.2
./bin/jobCommander localhost 18122 stop job_1 &
sleep 0.2
./bin/jobCommander localhost 18122 stop job_3 &
sleep 0.2
./bin/jobCommander localhost 18122 poll &
sleep 0.2
./bin/jobCommander localhost 18122 exit &
