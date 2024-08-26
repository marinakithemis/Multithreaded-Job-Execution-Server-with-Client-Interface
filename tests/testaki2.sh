valgrind --leak-check=full ./bin/jobExecutorServer 18121 3 5 &
sleep 4
./bin/jobCommander localhost 18121 issueJob ls -l /usr/bin/* /usr/local/bin/* /bin/* /sbin/* /opt/* /etc/* /usr/sbin/* 
./bin/jobCommander localhost 18121 exit 