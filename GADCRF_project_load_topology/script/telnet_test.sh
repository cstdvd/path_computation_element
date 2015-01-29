 #!/bin/bash/expect
 spawn telnet 10.1.1.1
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

