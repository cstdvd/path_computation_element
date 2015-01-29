 #!/bin/bash

echo user | sudo -S ifconfig tap0 up
echo ok1
echo user | sudo -S dhclient tap0
echo ok2

tap_addr=$(ifconfig tap0 | grep 'inet addr' | awk -F: '{print $2}' | awk '{print $1}')
echo ${tap_addr}
echo user | sudo -S route add default gw ${tap_addr} tap0

echo ok4


