#!/bin/bash
#
# sudo apt install mosquitto-clients moreutils
#
ip="192.168.10.141"
mdns="esp32-broker.local"
topic="#"
#mosquitto_sub -v -h ${ip} -p 1883  -t ${topic} | ts "%y/%m/%d %H:%M:%S"
mosquitto_sub -v -h ${mdns} -p 1883  -t ${topic} | ts "%y/%m/%d %H:%M:%S"
