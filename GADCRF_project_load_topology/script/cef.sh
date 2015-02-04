#!/bin/bash/expect
 set IP [lindex $argv 0]
 set N_ARG [llength $argv]
 spawn telnet $IP
 expect "Username:"
 send "admin\r"
 expect "Password:"
 send "admin\r"
 expect "*#"
 send "config t\r"
 expect "*(config)#"
 send "ip cef\r" 
 for {set i 1} {$i<$N_ARG} {incr i 1} {
 expect "*(config)#" 
 send "interface [lindex $argv $i]\r"
	expect "*(config-if)#"
	send "tag-switching ip\r"
	expect "*(config-if)#"
	send "exit\r"
 }
 expect "*(config)#"
 send "exit\r"
 expect "*#"
 send "exit\r"
