/* MQTT Broker Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "mongoose.h"

/* This project use WiFi configuration that you can set via 'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/

#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD

#if CONFIG_AP_MODE
#define EXAMPLE_MAX_STA_CONN       CONFIG_ESP_MAX_STA_CONN
#endif
#if CONFIG_ST_MODE
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY
#endif

#if CONFIG_ST_MODE
/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
#endif

/* The event group allows multiple bits for each event, but we only care about one event 
 * - are we connected to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;

static const char *TAG = "broker";


//static const char *s_listening_address = "0.0.0.0:1883";
static const char *s_listening_address = "1883";

static void mg_ev_handler(struct mg_connection *c, int ev, void *ev_data) {
  /* Do your custom event processing here */
  mg_mqtt_broker(c, ev, ev_data);

  switch (ev) {
    case MG_EV_POLL:
      ; //nothing to do
      break;
    case MG_EV_MQTT_CONNECT:
      ESP_LOGI(TAG, "MG_EV_MQTT_CONNECT");
      break;
    case MG_EV_MQTT_DISCONNECT:
      ESP_LOGI(TAG, "MG_EV_MQTT_DISCONNECT");
      break;
    case MG_EV_MQTT_PUBLISH:
      ESP_LOGI(TAG, "MG_EV_MQTT_PUBLISH");
      break;
    case MG_EV_MQTT_SUBSCRIBE:
      ESP_LOGI(TAG, "MG_EV_MQTT_SUBSCRIBE");
      break;
    case MG_EV_MQTT_UNSUBSCRIBE:
      ESP_LOGI(TAG, "MG_EV_MQTT_UNSUBSCRIBE");
      break;
    case MG_EV_MQTT_PINGREQ:
      ESP_LOGI(TAG, "MG_EV_MQTT_PINGREQ");
      mg_mqtt_pong(c); // Send PINGRESP
      break;
    case MG_EV_MQTT_PINGRESP:
      ESP_LOGI(TAG, "MG_EV_MQTT_PINGRESP");
      break;
    default:
      ; // ESP_LOGI(TAG, "MG_EV: %d", ev);
      break;
  }
}

static void event_handler(void* arg, esp_event_base_t event_base, 
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) ESP_LOGI(TAG, "WIFI_EVENT event_id=%d", event_id);
    if (event_base == IP_EVENT) ESP_LOGI(TAG, "IP_EVENT event_id=%d", event_id);

#if CONFIG_AP_MODE
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
#endif

#if CONFIG_ST_MODE
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:%s",
                 ip4addr_ntoa(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
#endif
}

#if CONFIG_AP_MODE
void wifi_init_softap()
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    //ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}
#endif

#if CONFIG_ST_MODE
bool parseAddress(int * ip, char * text) {
    ESP_LOGD(TAG, "parseAddress text=[%s]",text);
    int len = strlen(text);
    int octet = 0;
    char buf[4];
    int index = 0;
    for(int i=0;i<len;i++) {
        char c = text[i];
        if (c == '.') {
            ESP_LOGD(TAG, "buf=[%s] octet=%d", buf, octet);
            ip[octet] = strtol(buf, NULL, 10);
            octet++;
            index = 0;
        } else {
            if (index == 3) return false;
            if (c < '0' || c > '9') return false;
            buf[index++] = c;
            buf[index] = 0;
        }
    }

    if (strlen(buf) > 0) {
        ESP_LOGD(TAG, "buf=[%s] octet=%d", buf, octet);
        ip[octet] = strtol(buf, NULL, 10);
        octet++;
    }
    if (octet != 4) return false;
    return true;

}

void wifi_init_sta()
{
    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();

#if CONFIG_STATIC_IP

    ESP_LOGI(TAG, "CONFIG_STATIC_IP_ADDRESS=[%s]",CONFIG_STATIC_IP_ADDRESS);
    ESP_LOGI(TAG, "CONFIG_STATIC_GW_ADDRESS=[%s]",CONFIG_STATIC_GW_ADDRESS);
    ESP_LOGI(TAG, "CONFIG_STATIC_NM_ADDRESS=[%s]",CONFIG_STATIC_NM_ADDRESS);

    int ip[4];
    bool ret = parseAddress(ip, CONFIG_STATIC_IP_ADDRESS);
    ESP_LOGI(TAG, "parseAddress ret=%d ip=%d.%d.%d.%d", ret, ip[0], ip[1], ip[2], ip[3]);
    if (!ret) {
        ESP_LOGE(TAG, "CONFIG_STATIC_IP_ADDRESS [%s] not correct", CONFIG_STATIC_IP_ADDRESS);
	while(1) { vTaskDelay(1); }
    }

    int gw[4];
    ret = parseAddress(gw, CONFIG_STATIC_GW_ADDRESS);
    ESP_LOGI(TAG, "parseAddress ret=%d gw=%d.%d.%d.%d", ret, gw[0], gw[1], gw[2], gw[3]);
    if (!ret) {
        ESP_LOGE(TAG, "CONFIG_STATIC_GW_ADDRESS [%s] not correct", CONFIG_STATIC_GW_ADDRESS);
	while(1) { vTaskDelay(1); }
    }

    int nm[4];
    ret = parseAddress(nm, CONFIG_STATIC_NM_ADDRESS);
    ESP_LOGI(TAG, "parseAddress ret=%d nm=%d.%d.%d.%d", ret, nm[0], nm[1], nm[2], nm[3]);
    if (!ret) {
        ESP_LOGE(TAG, "CONFIG_STATIC_NM_ADDRESS [%s] not correct", CONFIG_STATIC_NM_ADDRESS);
	while(1) { vTaskDelay(1); }
    }

    tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);

    /* Set STATIC IP Address */
    tcpip_adapter_ip_info_t ipInfo;
    //IP4_ADDR(&ipInfo.ip, 192,168,10,100);
    IP4_ADDR(&ipInfo.ip, ip[0], ip[1], ip[2], ip[3]);
    IP4_ADDR(&ipInfo.gw, gw[0], gw[1], gw[2], gw[3]);
    IP4_ADDR(&ipInfo.netmask, nm[0], nm[1], nm[2], nm[3]);
    tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfo);

#endif

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);

    // wait for IP_EVENT_STA_GOT_IP
    while(1) {
        /* Wait forever for WIFI_CONNECTED_BIT to be set within the event group.
           Clear the bits beforeexiting. */
        EventBits_t uxBits = xEventGroupWaitBits(s_wifi_event_group,
           WIFI_CONNECTED_BIT, /* The bits within the event group to waitfor. */
           pdTRUE,        /* WIFI_CONNECTED_BIT should be cleared before returning. */
           pdFALSE,       /* Don't waitfor both bits, either bit will do. */
           portMAX_DELAY);/* Wait forever. */
       if ( ( uxBits & WIFI_CONNECTED_BIT ) == WIFI_CONNECTED_BIT ){
           ESP_LOGI(TAG, "WIFI_CONNECTED_BIT");
           break;
       }
    }
    ESP_LOGI(TAG, "Got IP Address.");
}
#endif

void app_main()
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
#if CONFIG_AP_MODE
    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();
    tcpip_adapter_ip_info_t ip_info;
    ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info));
    ESP_LOGI(TAG, "ESP32 is AP MODE");
#endif

#if CONFIG_ST_MODE
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    tcpip_adapter_ip_info_t ip_info;
    ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
    ESP_LOGI(TAG, "ESP32 is STA MODE");
#endif

    /* Starting Mongoose */
    struct mg_mgr mgr;
    struct mg_connection *nc;
    struct mg_mqtt_broker brk;

    mg_mgr_init(&mgr, NULL);

    nc = mg_bind(&mgr, s_listening_address, mg_ev_handler);
    if (nc == NULL) {
      ESP_LOGE(TAG, "Error setting up listener!");
      return;
    }

    mg_mqtt_broker_init(&brk, NULL);
    nc->priv_2 = &brk;
    mg_set_protocol_mqtt(nc);

    /* Print the local IP address */
    ESP_LOGI(TAG, "MQTT broker started on %s", s_listening_address);
    ESP_LOGI(TAG, "IP Address:  %s", ip4addr_ntoa(&ip_info.ip));
    ESP_LOGI(TAG, "Subnet mask: %s", ip4addr_ntoa(&ip_info.netmask));
    ESP_LOGI(TAG, "Gateway:     %s", ip4addr_ntoa(&ip_info.gw));

    /* Processing events */
    while (1) {
      mg_mgr_poll(&mgr, 1000);
    }

}
