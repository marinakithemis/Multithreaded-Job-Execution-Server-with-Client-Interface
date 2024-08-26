valgrind --leak-check=full ./bin/jobExecutorServer 18123 1 2 &
sleep 4
./bin/jobCommander localhost 18123 issueJob ./tests/progDelay 1000 &
sleep 0.2
./bin/jobCommander localhost 18123 issueJob ./tests/progDelay 110 &
sleep 0.2
./bin/jobCommander localhost 18123 issueJob ./tests/progDelay 115 &
sleep 0.2
./bin/jobCommander localhost 18123 issueJob ./tests/progDelay 120 &
sleep 0.2
./bin/jobCommander localhost 18123 issueJob ./tests/progDelay 125 &
sleep 0.2
./bin/jobCommander localhost 18123 poll &
sleep 0.2
./bin/jobCommander localhost 18123 setConcurrency 2 &
sleep 0.2
./bin/jobCommander localhost 18123 poll &
sleep 0.2
./bin/jobCommander localhost 18123 exit &
sleep 2
killall ./tests/progDelay