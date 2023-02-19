#!/bin/bash
#
# sudo apt install mosquitto-clients moreutils
#

#set -x
mdns="esp32-broker.local"
topic="#"
mosquitto_sub -v -h ${mdns} -p 1883  -t ${topic} | ts "%y/%m/%d %H:%M:%S"
