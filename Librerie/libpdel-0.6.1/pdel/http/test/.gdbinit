file demo_server
set args -p 8080 -r http://www.foobar.com
handle SIGUSR1 pass nostop noprint
handle SIGPIPE pass nostop noprint
handle SIGTERM pass nostop noprint
#set args http://my.yahoo.com/
#set args -s https://investing.schwab.com/trading/start
#set args -c 3 http://my.yahoo.com/
run
