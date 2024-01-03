#!/bin/bash
#
# sudo apt install mosquitto-clients moreutils
#

#set -x
mdns="esp32-broker.local"
while :
do
	payload=`date "+%Y/%m/%d-%H:%M:%S"`
	mosquitto_pub -h ${mdns} -p 1883 -t "topic" -m ${payload}
	sleep 1
done
