 #!/bin/bash

echo user | sudo -S ifconfig tap0 up

echo user | sudo -S dhclient tap0


