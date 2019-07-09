# esp-idf-mqtt-broker
MQTT Broker for esp-idf.   
This project use [Mongoose networking library](https://cesanta.com/docs/overview/intro.html).   
I forked from [here](https://github.com/bigw00d/esp32_mongoose_sample).   
Your folk is welcome.   

---

1. In this project directory, create a components directory.

2. In the components directory, clone Mongoose:
git clone https://github.com/cesanta/mongoose.git

3. In the new Mongoose directory, create a component.mk file containing:

```
COMPONENT_ADD_INCLUDEDIRS=.
```

4. You have to enable MG_ENABLE_MQTT_BROKER  in mongoose.h.

```
#ifndef MG_ENABLE_MQTT_BROKER
#define MG_ENABLE_MQTT_BROKER 1
#endif
```

5. Compile this project.

---

```
git clone https://github.com/nopnop2002/esp-idf-mqtt-broker
cd esp-idf-mqtt-broker
mkdir -p components
cd components/
git clone https://github.com/cesanta/mongoose.git
cd mongoose/
echo "COMPONENT_ADD_INCLUDEDIRS=." > component.mk
vi mongoose.h
Enable MG_ENABLE_MQTT_BROKER
cd ../..
make menuconfig
make flash
```

---

# Wifi Setting

You can choice Wifi setting.   

![config-menu](https://user-images.githubusercontent.com/6020549/60885379-6ed2b500-a28a-11e9-9c1f-b56b0b0223ec.jpg)

## Access Point Mode
![config-ap](https://user-images.githubusercontent.com/6020549/60885370-69756a80-a28a-11e9-974d-123cb290de3f.jpg)

SSID:SSID of ESP32   
ESP32 have 192.168.4.1.   

## Station Mode
![config-sta](https://user-images.githubusercontent.com/6020549/60885405-78f4b380-a28a-11e9-8709-3b7ef1f1a903.jpg)

SSID:SSID of your Wifi router   
ESP32 get IP using DHCP.    

## Station Mode of Static Address
![config-static](https://user-images.githubusercontent.com/6020549/60885411-7befa400-a28a-11e9-8871-cf6e3c6ee96a.jpg)

SSID:SSID of your Wifi router   
ESP32 set your specific IP.   

----

# Monitor

## Accesas Point Mode
![ap_mode](https://user-images.githubusercontent.com/6020549/60885576-e6084900-a28a-11e9-89ab-b7d22e4a376e.jpg)

## Station Mode
![sta_mode](https://user-images.githubusercontent.com/6020549/60885628-f7e9ec00-a28a-11e9-8941-01303a1c84d0.jpg)

