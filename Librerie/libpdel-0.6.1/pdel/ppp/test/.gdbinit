file demo_server
set args -e 128 -s 3.3.3.3 -p 2.2.2.2 -U test -P test
handle SIGUSR1 pass nostop noprint
handle SIGPIPE pass nostop noprint
handle SIGTERM pass nostop noprint
break main
run
