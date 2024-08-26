valgrind --leak-check=full ./bin/jobExecutorServer 18127 3 5 &
sleep 4
./bin/jobCommander localhost 18127 issueJob ./tests/progDelay 1000 &
sleep 0.2
./bin/jobCommander localhost 18127 stop job_2 &
sleep 0.2
./bin/jobCommander localhost 18127 issueJob ./tests/progDelay 5 &
sleep 0.2
./bin/jobCommander localhost 18127 issueJob ./tests/progDelay 5 &
sleep 0.2
./bin/jobCommander localhost 18127 issueJob ./tests/progDelay 5 &
sleep 0.2
./bin/jobCommander localhost 18127 poll &
sleep 0.2
./bin/jobCommander localhost 18127 setConcurrency 2 &
sleep 0.2
./bin/jobCommander localhost 18127 issueJob ./tests/progDelay 5 &
sleep 0.2
./bin/jobCommander localhost 18127 issueJob ./tests/progDelay 5 &
sleep 0.2
./bin/jobCommander localhost 18127 poll &
sleep 3
killall ./tests/progDelay
./bin/jobCommander localhost 18127 exit &

