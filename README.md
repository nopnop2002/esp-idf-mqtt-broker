# esp-idf-mqtt-broker
MQTT Broker for esp-idf.   
This project use [Mongoose networking library](https://cesanta.com/docs/overview/intro.html).   
I forked from [here](https://github.com/bigw00d/esp32_mongoose_sample).   
Your fork is welcome.   

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

![config-1](https://user-images.githubusercontent.com/6020549/93414263-e6af5d00-f8db-11ea-903e-155e614709be.jpg)

## Access Point Mode
![config-4](https://user-images.githubusercontent.com/6020549/93414276-e8792080-f8db-11ea-99f6-df1bcf991487.jpg)

SSID:SSID of ESP32   
ESP32 have 192.168.4.1.   

## Station Mode
![config-2](https://user-images.githubusercontent.com/6020549/93414269-e7e08a00-f8db-11ea-95f0-3de34ef65638.jpg)

SSID:SSID of your Wifi router   
ESP32 get IP using DHCP.    

## Station Mode of Static Address
![config-3](https://user-images.githubusercontent.com/6020549/93414274-e8792080-f8db-11ea-897e-ed2dec19a8eb.jpg)

SSID:SSID of your Wifi router   
ESP32 set your specific IP.   

----

# Using MDNS hostname
You can use the MDNS hostname instead of the IP address.   

![mdns-2](https://user-images.githubusercontent.com/6020549/93414486-5f161e00-f8dc-11ea-8a1b-8b0b050c2be4.jpg)

![mdns-1](https://user-images.githubusercontent.com/6020549/93414481-5de4f100-f8dc-11ea-817d-5c4350a8ad09.jpg)

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

# MQTT Broker over web sockets

![config-5](https://user-images.githubusercontent.com/6020549/93414277-e911b700-f8db-11ea-8045-1251788906e8.jpg)

You can test WebSocket using test-socket.py.   
I forked [this](http://www.steves-internet-guide.com/download/websockets-publish-subscribe/).   

```
curl https://bootstrap.pypa.io/get-pip.py -o - | sudo python
sudo pip install paho-mqtt
python ./test-socket.py
```
