 #!/bin/bash/expect
 set IP [lindex $argv 0]
 spawn telnet $IP
 expect "Username:"
 send "admin\r"
 expect "Password:"
 send "admin\r"
 expect "*#"
 send "config t\r"
 expect "*(config)#"
 send "exit\r"
 expect "*#"
 send "show running-config\r"
 expect "*#"

