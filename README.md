# esp-idf-mqtt-broker
MQTT Broker for esp-idf.   
This project use [Mongoose networking library](https://github.com/cesanta/mongoose).   
I forked from [here](https://github.com/bigw00d/esp32_mongoose_sample).   
I tested  mongoose VERSION 7.2.   
Your fork is welcome.   


# Installation overview

1. In this project directory, create a components directory.

2. In the components directory, clone Mongoose:
```
git clone https://github.com/cesanta/mongoose.git
```

3. In the new Mongoose directory, create a CMakeLists.txt file containing:
```
idf_component_register(SRCS "mongoose.c" INCLUDE_DIRS ".")
```

4. Compile this project.


# Installation for ESP32
```
git clone https://github.com/nopnop2002/esp-idf-mqtt-broker
cd esp-idf-mqtt-broker
mkdir -p components
cd components/
git clone https://github.com/cesanta/mongoose.git
cd mongoose/
echo "idf_component_register(SRCS \"mongoose.c\" INCLUDE_DIRS \".\")" > CMakeLists.txt
cd ../..
idf.py set-target esp32
idf.py menuconfig
idf.py flash monitor
```

# Installation for ESP32-S2
```
git clone https://github.com/nopnop2002/esp-idf-mqtt-broker
cd esp-idf-mqtt-broker
mkdir -p components
cd components/
git clone https://github.com/cesanta/mongoose.git
cd mongoose/
echo "idf_component_register(SRCS \"mongoose.c\" INCLUDE_DIRS \".\")" > CMakeLists.txt
cd ../..
idf.py set-target esp32s2
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


## Using MDNS hostname
You can use the MDNS hostname instead of the IP address.   
You need to change the mDNS strict mode according to [this](https://github.com/espressif/esp-idf/issues/6190) instruction.   

![mdns-1](https://user-images.githubusercontent.com/6020549/93420660-60e6de00-f8ea-11ea-9783-3c295130a840.jpg)

![mdns-2](https://user-images.githubusercontent.com/6020549/93420837-cdfa7380-f8ea-11ea-952c-64113c929df7.jpg)

You can change MDNS hostname using menuconfig.   

![config-5](https://user-images.githubusercontent.com/6020549/110200318-a56a3400-7ea0-11eb-8dfb-b07bbb03b0f1.jpg)

## Start MQTT Subscriber
![config-6](https://user-images.githubusercontent.com/6020549/110200319-a56a3400-7ea0-11eb-9a42-1c3543c9b802.jpg)

## Start MQTT Publisher
![config-7](https://user-images.githubusercontent.com/6020549/110200403-2b867a80-7ea1-11eb-9d07-80fefa3d4b34.jpg)

# Limitations
- will topics   
will qos and will retain are ignored.   

- Unsupported MQTT request   
Do not respond to these MQTT requests:   
PUBREC   
PUBREL   
PUBCOMP   

# Subscribe using mosquitto-clients
```
$ sudo apt install mosquitto-clients moreutils
$ chmod 777 ./mqtt_sub.sh
$ ./mqtt_sub.sh
21/03/06 18:14:19 esp32 TickCount=110964
21/03/06 18:14:20 esp32 TickCount=111065
21/03/06 18:14:21 esp32 TickCount=111166
21/03/06 18:14:22 esp32 TickCount=111267
21/03/06 18:14:23 esp32 TickCount=111368
```

# Publish using mosquitto-clients
```
$ sudo apt install mosquitto-clients moreutils
$ chmod 777 ./mqtt_sub.sh
$ ./mqtt_pub.sh
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

