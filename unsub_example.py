#!usr/bin/env python
# -*- coding: utf-8 -*-
#
# python2
# pip install paho-mqtt
#
# python3
# pip3 install paho-mqtt

import paho.mqtt.client as mqtt     # MQTTのライブラリをインポート

# ブローカーに接続できたときの処理
def on_connect(client, userdata, flag, rc):
  print("Connected with result code " + str(rc))  # 接続できた旨表示
  print("subscribe hoge");
  client.subscribe("hoge/#")  # subするトピックを設定
  client.subscribe("system/#")  # subするトピックを設定

# ブローカーが切断したときの処理
def on_disconnect(client, userdata, flag):
  print("disconnection.")

# メッセージが届いたときの処理
def on_message(client, userdata, msg):
  # msg.topicにトピック名が，msg.payloadに届いたデータ本体が入っている
  print("Received message '" + str(msg.payload) + "' on topic '" + msg.topic + "' with QoS " + str(msg.qos))
  topics = msg.topic
  topics = topics.split('/')
  #print("topics={}".format(topics))
  if (topics[0] == "hoge"):
    print("unsubscribe hoge, subscribe fuga");
    client.unsubscribe("hoge/#")    # unsubscribe
    client.subscribe("fuga/#")      # subscribe
  if (topics[0] == "fuga"):
    print("unsubscribe fuga, subscribe hoge");
    client.unsubscribe("fuga/#")    # unsubscribe
    client.subscribe("hoge/#")      # subscribe

# MQTTの接続設定
client = mqtt.Client()                 # クラスのインスタンス(実体)の作成
client.on_connect = on_connect         # 接続時のコールバック関数を登録
client.on_disconnect = on_disconnect   # 切断時のコールバックを登録
client.on_message = on_message         # メッセージ到着時のコールバック

client.will_set('system/message', 'Subscriber Down')
#client.connect("192.168.10.141", 1883, 60)  # 接続先は自分自身
client.connect("esp32-broker.local", 1883, 60)  # 接続先は自分自身

client.loop_forever()                  # 永久ループして待ち続ける
