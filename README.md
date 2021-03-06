# esp-idf-mqtt-broker
MQTT Broker for esp-idf.   
This project use [Mongoose networking library](https://github.com/cesanta/mongoose).   
I forked from [here](https://github.com/bigw00d/esp32_mongoose_sample).   
I tested  MG_VERSION "7.2".   
Your fork is welcome.   


# Installation overview

1. In this project directory, create a components directory.

2. In the components directory, clone Mongoose.
git clone https://github.com/cesanta/mongoose.git

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
idf.py flash
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
idf.py flash
```

# Application Setting

![config-1](https://user-images.githubusercontent.com/6020549/110200312-a307da00-7ea0-11eb-85fa-c76f932b8023.jpg)

You can choice Wifi setting.   

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
The current mg_strcmp() function does not support wildcard topics.   
So I replaced this with _mg_strcmp() function.   
The _mg_strcmp() function does not support the "+" wildcard.   
