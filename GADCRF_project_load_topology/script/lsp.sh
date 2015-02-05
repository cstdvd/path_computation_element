#!/bin/bash/expect
#sintassi -> IP dest capacità id path
 set IP [lindex $argv 0]
 set DEST [lindex $argv 1]
 set C [lindex $argv 2]
 set ID [lindex $argv 3]
 set N_ARG [llength $argv]
 spawn telnet $IP
 expect "Username:"
 send "admin\r"
 expect "Password:"
 send "admin\r"
 expect "*#"
 send "config t\r"
 expect "*(config)#"
 send "interface Tunnel$ID\r"
 expect "*(config-if)#"
 send "ip unnumbered Loopback0\r"
 expect "*(config-if)#"
 send "tunnel destination $DEST\r"
 expect "*(config-if)#"
 send "tunnel mode mpls traffic-eng\r"
 expect "*(config-if)#"
 send "tunnel mpls traffic-eng autoroute announce\r"
 expect "*(config-if)#"
 send "tunnel mpls traffic-eng priority 2 2\r"
 expect "*(config-if)#"
 send "tunnel mpls traffic-eng bandwidth $C\r"
 expect "*(config-if)#"
 send "tunnel mpls traffic-eng path-option 1 explicit name path$ID\r"
 expect "*(config-if)#"
 send "ip explicit-path name path$ID enable\r"
 for {set i 4} {$i<$N_ARG} {incr i 1} {
 	expect "*(cfg-ip-expl-path)#"
	send "next-address [lindex $argv $i]\r"
 }
 expect "*(cfg-ip-expl-path)#"
 send "exit\r"
 expect "*(config)#"
 send "exit\r"
 expect "*#"
 send "exit\r"
 
 
