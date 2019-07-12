/* MQTT Broker using tcp transport

   This code is in the Public Domain (or CC0 licensed, at your option.)

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

extern SemaphoreHandle_t xSemaphore_subscriber;
extern char self_address[];

static const char *TAG = "tcp";

//static const char *s_listening_address = "0.0.0.0:1883";

static void broker_handler(struct mg_connection *c, int ev, void *ev_data) {
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


void tcp_server(void *pvParameters)
{
    /* Starting Broker */
    struct mg_mgr mgr;
    struct mg_connection *nc;
    struct mg_mqtt_broker brk;
    char server_address[32];
    ESP_LOGD(TAG, "self_address=[%s]", self_address);
    sprintf(server_address, "%s:1883", self_address);
    ESP_LOGI(TAG, "MQTT broker started on %s", server_address);

    mg_mgr_init(&mgr, NULL);

    //nc = mg_bind(&mgr, s_listening_address, broker_handler);
    nc = mg_bind(&mgr, server_address, broker_handler);
    if (nc == NULL) {
      ESP_LOGE(TAG, "Error setting up listener!");
      while(1) { vTaskDelay(1); }
    }

    mg_mqtt_broker_init(&brk, NULL);
    nc->priv_2 = &brk;
    mg_set_protocol_mqtt(nc);

    /* Start Subscriber */ 
    xSemaphoreGive(xSemaphore_subscriber);

    /* Processing events */
    while (1) {
      //mg_mgr_poll(&mgr, 1000);
      mg_mgr_poll(&mgr, 0);
      vTaskDelay(1);
    }

}

