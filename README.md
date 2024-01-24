# esp-idf-mqtt-broker
MQTT Broker for esp-idf.   
This project use [Mongoose networking library](https://github.com/cesanta/mongoose).   
I forked from [here](https://github.com/bigw00d/esp32_mongoose_sample).   
However, with the release of mongoose ver7, I rewrote it.   
Your fork is welcome.   


# Installation overview

1. In this project directory, create a components directory.

2. In the components directory, clone Mongoose version 7.9:
```
git clone -b 7.9 https://github.com/cesanta/mongoose.git
```

3. In the new Mongoose directory, create a CMakeLists.txt file containing:
```
idf_component_register(SRCS "mongoose.c" PRIV_REQUIRES esp_timer INCLUDE_DIRS ".")
```

4. Compile this project.

# Software requiment
- mongoose version 7.9.   
 The version of mongoose is written [here](https://github.com/cesanta/mongoose/blob/master/mongoose.h#L23).   

- ESP-IDF V4.4/V5.x.   
 ESP-IDF V5.0 is required when using ESP32-C2.   
 ESP-IDF V5.1 is required when using ESP32-C6.   


# Installation
```
git clone https://github.com/nopnop2002/esp-idf-mqtt-broker
cd esp-idf-mqtt-broker
mkdir components
cd components/
git clone -b 7.9 https://github.com/cesanta/mongoose.git
cd mongoose/
echo "idf_component_register(SRCS \"mongoose.c\" PRIV_REQUIRES esp_timer INCLUDE_DIRS \".\")" > CMakeLists.txt
cd ../..
idf.py set-target {esp32/esp32s2/esp32s3/esp32c2/esp32c3/esp32c6}
idf.py menuconfig
idf.py flash monitor
```


# Application Setting

![config-1](https://user-images.githubusercontent.com/6020549/110200312-a307da00-7ea0-11eb-85fa-c76f932b8023.jpg)

## Station Mode
![config-2](https://user-images.githubusercontent.com/6020549/110200315-a4390700-7ea0-11eb-8021-f8355818fbb2.jpg)

SSID:SSID of your Wifi router   
ESP32 get IP using DHCP.    

## Station Mode of Static Address
![config-3](https://user-images.githubusercontent.com/6020549/110200316-a4390700-7ea0-11eb-9266-473ad7fb193e.jpg)

SSID:SSID of your Wifi router   
ESP32 set your specific IP.   

## Access Point Mode
![config-4](https://user-images.githubusercontent.com/6020549/110200317-a4d19d80-7ea0-11eb-84ec-21f78f97930b.jpg)

SSID:SSID of ESP32   
ESP32 have 192.168.4.1.   


## Using mDNS hostname
You can connect using the mDNS hostname instead of the IP address.   
- esp-idf V4.3 or earlier   
 You will need to manually change the mDNS strict mode according to [this](https://github.com/espressif/esp-idf/issues/6190) instruction.   
- esp-idf V4.4  
 If you set CONFIG_MDNS_STRICT_MODE = y in sdkconfig.defaults, the firmware will be built with MDNS_STRICT_MODE.   
 __If MDNS_STRICT_MODE is not set, mDNS name resolution will not be possible after long-term operation.__   
- esp-idf V4.4.1   
 mDNS component has been updated.   
 If you set CONFIG_MDNS_STRICT_MODE = y in sdkconfig.defaults, the firmware will be built with MDNS_STRICT_MODE.   
 __Even if MDNS_STRICT_MODE is set, mDNS name resolution will not be possible after long-term operation.__   
- esp-idf V5.0 or later   
 mDNS component has been updated.   
 Long-term operation is possible without setting MDNS_STRICT_MODE.   
 The following lines in sdkconfig.defaults should be removed before menuconfig.   
 ```CONFIG_MDNS_STRICT_MODE=y```

![mdns-1](https://user-images.githubusercontent.com/6020549/93420660-60e6de00-f8ea-11ea-9783-3c295130a840.jpg)

![mdns-2](https://user-images.githubusercontent.com/6020549/93420837-cdfa7380-f8ea-11ea-952c-64113c929df7.jpg)

You can change mDNS hostname using menuconfig.   

![config-5](https://user-images.githubusercontent.com/6020549/110200318-a56a3400-7ea0-11eb-8dfb-b07bbb03b0f1.jpg)

## Start Built-In MQTT Subscriber
![config-6](https://user-images.githubusercontent.com/6020549/110200319-a56a3400-7ea0-11eb-9a42-1c3543c9b802.jpg)

## Start Built-In MQTT Publisher
![config-7](https://user-images.githubusercontent.com/6020549/110200403-2b867a80-7ea1-11eb-9d07-80fefa3d4b34.jpg)

# Limitations
- will topics   
will qos and will retain are ignored.   

- Unsupported MQTT request   
Do not respond to these MQTT requests:   
PUBREC   
PUBREL   
PUBCOMP   

# Publish using mosquitto-clients
```
$ sudo apt install mosquitto-clients moreutils
$ chmod 777 ./mqtt_sub.sh
$ ./mqtt_pub.sh
```

# Subscribe using mosquitto-clients
```
$ sudo apt install mosquitto-clients moreutils
$ chmod 777 ./mqtt_sub.sh
$ ./mqtt_sub.sh
23/02/19 12:35:16 topic 2023/02/19-12:35:16
23/02/19 12:35:17 topic 2023/02/19-12:35:17
23/02/19 12:35:18 topic 2023/02/19-12:35:18
23/02/19 12:35:19 topic 2023/02/19-12:35:19
23/02/19 12:35:21 topic 2023/02/19-12:35:20
```

# Notify topic of will using mosquitto-clients
- In Terminal #1, do the following:   
```
$ mosquitto_sub -v -h esp32-broker.local -p 1883  -t "topic/#" --will-topic "topic/will" --will-payload "GOODBYE"
```

- Open new terminal. In Terminal #2, do the following:   
```
$ mosquitto_sub -v -h esp32-broker.local -p 1883  -t "topic/#"
```

- Press Control+C in Terminal #1:   

- The following is displayed in Terminal #2:   
```
topic/will GOODBYE
```

# Unsubscribe using python and paho
```
$ pip install paho
$ python unsub_example.py

Connected with result code 0
subscribe hoge

Received message 'b'test'' on topic 'hoge/1' with QoS 1
unsubscribe hoge, subscribe fuga

Received message 'b'test'' on topic 'fuga/1' with QoS 1
unsubscribe fuga, subscribe hoge
```

First, it subscribes to the topic [hoge/#].   
When it receives the topic [hoge/1], it unsubscribes [hoge/#] and subscribes to [fuga/#].   
When it receives the topic [fuga/1], it unsubscribes [fuga/#] and subscribes to [hoge/#] again.   

# Screen Shot
The message flows like this:   
PUBLISH->BROKER->SUBSCRIBE   
![ScreenShot](https://user-images.githubusercontent.com/6020549/110209284-e2e5b600-7ece-11eb-8fb3-941e6fdf56c6.jpg)

