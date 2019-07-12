/* MQTT Broker over web sockets

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

extern SemaphoreHandle_t xSemaphore_subscriber;
//extern char self_address[32];
extern char self_address[];

char s_mqtt_address[32];

static const char *TAG = "web_socket";

void mqtt_server(void *pvParameters);

static void unproxy(struct mg_connection *c) {
  struct mg_connection *pc = (struct mg_connection *) c->user_data;
  if (pc != NULL) {
    pc->flags |= MG_F_CLOSE_IMMEDIATELY;
    pc->user_data = NULL;
    c->user_data = NULL;
  }
  ESP_LOGI(TAG, "Closing connection %p", c);
}

static void proxy_handler(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_POLL) return;
  ESP_LOGD(TAG, "%p %s EVENT %d %p", c, __func__, ev, ev_data);
  switch (ev) {
    case MG_EV_CLOSE: {
      unproxy(c);
      break;
    }
    case MG_EV_RECV: {
      struct mg_connection *pc = (struct mg_connection *) c->user_data;
      if (pc != NULL) {
        ESP_LOGI(TAG, "Responding %d bytes", (int) c->recv_mbuf.len);
        ESP_LOG_BUFFER_HEX_LEVEL("", c->recv_mbuf.buf, c->recv_mbuf.len, ESP_LOG_INFO);


        // Parse each response
        char data[64];
        size_t data_len;
        int index = 0;
        while(1) {
          data_len = c->recv_mbuf.buf[index+1];
          memcpy(data, (char *)&c->recv_mbuf.buf[index], data_len+2);
          ESP_LOGI(TAG, "index=%d data_len=%d", index,data_len);
          ESP_LOG_BUFFER_HEX_LEVEL("", data, data_len+2, ESP_LOG_INFO);
          mg_send_websocket_frame(pc, WEBSOCKET_OP_BINARY, data, data_len+2);
          index = index + data_len+2;
          if (index >= c->recv_mbuf.len) break;
        }
         
#if 0
        mg_send_websocket_frame(pc, WEBSOCKET_OP_BINARY, c->recv_mbuf.buf,
                                c->recv_mbuf.len);
#endif
        mbuf_remove(&c->recv_mbuf, c->recv_mbuf.len);
      }
      break;
    }
  }
}

static void http_handler(struct mg_connection *c, int ev, void *ev_data) {
  struct mg_connection *pc = (struct mg_connection *) c->user_data;
  if (ev == MG_EV_POLL) return;
  ESP_LOGD(TAG, "%p %s EVENT %d %p", c, __func__, ev, ev_data);
  /* Do your custom event processing here */
  switch (ev) {
    case MG_EV_WEBSOCKET_HANDSHAKE_DONE: {
      ESP_LOGI(TAG, "MG_EV_WEBSOCKET_HANDSHAKE_DONE");
      pc = mg_connect(c->mgr, s_mqtt_address, proxy_handler);
      pc->user_data = c;
      c->user_data = pc;
      ESP_LOGI(TAG, "Created proxy connection %p", pc);
      break;
    }
    case MG_EV_WEBSOCKET_FRAME: {
      ESP_LOGI(TAG, "MG_EV_WEBSOCKET_FRAME");
      struct websocket_message *wm = (struct websocket_message *) ev_data;
      if (pc != NULL) {
        uint16_t command = (wm->data[0] >> 4) & 0x0f;
        ESP_LOGI(TAG, "wm->data[0]=[%02x] command=[%02x]", wm->data[0], command);
        ESP_LOGI(TAG, "wm->data[1]=%d wm->size=%d", wm->data[1], wm->size);
        if (wm->size == wm->data[1]+2) {
          if ( command == 0x0f) {
            ESP_LOGW(TAG, "Illegal command");
          } else {
            ESP_LOGI(TAG, "Forwarding %d bytes", (int) wm->size);
            ESP_LOG_BUFFER_HEX_LEVEL("", wm->data, wm->size, ESP_LOG_INFO);
            mg_send(pc, wm->data, wm->size);
          }
        } else {
          ESP_LOGW(TAG, "Illegal length");
          esp_log_buffer_hex("",wm->data, wm->size);
        }
      }
      break;
    }
    case MG_EV_CLOSE: {
      unproxy(c);
      break;
    }
  }
}

static void mqtt_handler(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_POLL) return;
  ESP_LOGD(TAG, "%p %s EVENT %d %p", c, __func__, ev, ev_data);
  /* Do your custom event processing here */
  switch (ev) {
    case MG_EV_CLOSE:
      ESP_LOGI(TAG, "Closing MQTT connection %p\n", c);
      break;
  }
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

//static void start_mqtt_server(struct mg_mgr *mgr, const char *addr) {
void mqtt_server(void *pvParameters)
{
    /* Starting Broker */
    struct mg_mgr mgr;
    struct mg_connection *nc;
    struct mg_mqtt_broker brk;
    ESP_LOGI(TAG, "MQTT broker started on %s", s_mqtt_address);
    mg_mgr_init(&mgr, NULL);

    nc = mg_bind(&mgr, s_mqtt_address, mqtt_handler);
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


//static void start_http_server(struct mg_mgr *mgr, const char *addr) {
void http_server(void *pvParameters)
{
    /* Starting HTTP Server */
    struct mg_mgr mgr;
    struct mg_connection *nc;
    char server_address[32];
    ESP_LOGD(TAG, "self_address=[%s]", self_address);
    sprintf(server_address, "%s:8080", self_address);
    ESP_LOGI(TAG, "HTTP server started on %s", server_address);

    mg_mgr_init(&mgr, NULL);

    nc = mg_bind(&mgr, server_address, http_handler);
    if (nc == NULL) {
      ESP_LOGE(TAG, "Error setting up listener!");
      while(1) { vTaskDelay(1); }
    }

    mg_set_protocol_http_websocket(nc);

#if 1
    sprintf(s_mqtt_address, "%s:1883", self_address);
    xTaskCreate(mqtt_server, "MQTT", 1024*4, NULL, 2, NULL);
#endif

    /* Processing events */
    while (1) {
      //mg_mgr_poll(&mgr, 1000);
      mg_mgr_poll(&mgr, 0);
      vTaskDelay(1);
    }
}

#if 0
void web_socket(void *pvParameters)
{
  struct mg_mgr mgr;
  mg_mgr_init(&mgr, NULL);
  start_http_server(&mgr, s_http_address);
  start_mqtt_server(&mgr, s_mqtt_address);
  for (;;) {
    mg_mgr_poll(&mgr, 1000);
  }
}
#endif
