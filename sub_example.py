#!usr/bin/env python
# -*- coding: utf-8 -*-
#
# python3 -m pip install -U paho-mqtt

import argparse
import paho.mqtt
import paho.mqtt.client as mqtt

# Handler when connecting to broker
def on_connect(client, userdata, flag, rc):
	print("Connected with result code " + str(rc))
	client.subscribe("/system/#")

# Handler when broker disconnects
def on_disconnect(client, userdata, flag):
	print("disconnection.")

# # Handler when message arrives
def on_message(client, userdata, msg):
	# msg.topic contains the topic name and msg.payload contains the received data
	print("Received message '" + str(msg.payload) + "' on topic '" + msg.topic + "' with QoS " + str(msg.qos))

if __name__=='__main__':
	parser = argparse.ArgumentParser()
	parser.add_argument('--host', help='mqtt host', default="esp32-broker.local")
	parser.add_argument('--port', type=int, help='mqtt port', default=1883)
	args = parser.parse_args()
	print("args.host={}".format(args.host))
	print("args.port={}".format(args.port))

	client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
	client.on_connect = on_connect
	client.on_disconnect = on_disconnect
	client.on_message = on_message

	client.will_set('/system/message', 'Subscriber Down')
	client.connect(args.host, args.port, 60)

	# keep waiting in an endless loop
	client.loop_forever()
