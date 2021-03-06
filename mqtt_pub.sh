#!/bin/bash
#
# sudo apt install mosquitto-clients moreutils
#

#set -x
ip="192.168.10.141"
mdns="esp32-broker.local"
while :
do
	payload=`date "+%Y/%m/%d-%H:%M:%S"`
	#mosquitto_pub -h ${ip} -p 1883 -t "topic" -m ${payload}
	mosquitto_pub -h ${mdns} -p 1883 -t "topic" -m ${payload}
	sleep 1
done
