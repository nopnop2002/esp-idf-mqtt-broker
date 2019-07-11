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
![ap_mode](https://user-images.githubusercontent.com/6020549/61045044-ff3d0100-a414-11e9-9eae-effa8864a123.jpg)

SSID:SSID of ESP32   
ESP32 have 192.168.4.1.   

## Station Mode
![config-sta](https://user-images.githubusercontent.com/6020549/60974756-977bad00-a365-11e9-9298-961a1d6ffbf5.jpg)

SSID:SSID of your Wifi router   
ESP32 get IP using DHCP.    

## Station Mode of Static Address
![config-static](https://user-images.githubusercontent.com/6020549/60974754-977bad00-a365-11e9-8262-fc394c495393.jpg)

SSID:SSID of your Wifi router   
ESP32 set your specific IP.   

----

# Monitor

## Show all incoming message
![show_all](https://user-images.githubusercontent.com/6020549/60963697-7490cf00-a34c-11e9-8386-1025afce8f8f.jpg)

## Show event only
![show_event](https://user-images.githubusercontent.com/6020549/60963702-76f32900-a34c-11e9-9fbe-781f583965ab.jpg)

----

# Bug Fix to mongoose.c

There is some bug in mongoose.c.   
So you have to fix manually.   

```
void mg_mqtt_suback(struct mg_connection *nc, uint8_t *qoss, size_t qoss_len,
                    uint16_t message_id) {
  size_t i;
  uint16_t netbytes;

  //Comment by nopnop2002
  //mg_send_mqtt_header(nc, MG_MQTT_CMD_SUBACK, MG_MQTT_QOS(1), 2 + qoss_len);
  //Add by nopnop2002
  mg_send_mqtt_header(nc, MG_MQTT_CMD_SUBACK, 0, 2 + qoss_len);

  netbytes = htons(message_id);
  mg_send(nc, &netbytes, 2);

  for (i = 0; i < qoss_len; i++) {
    mg_send(nc, &qoss[i], 1);
  }
}
```

---

# MQTT Client
You can use [this](https://github.com/espressif/esp-idf/tree/master/examples/protocols/mqtt/tcp) as client.
