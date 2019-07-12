/* MQTT Broker Subscriber

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

static const char *TAG = "subscriber";

static const char *s_user_name = NULL;
static const char *s_password = NULL;
static const char *s_topic = "#";
static struct mg_mqtt_topic_expression s_topic_expr = {NULL, 0};

extern char self_address[];

static void client_handler(struct mg_connection *nc, int ev, void *p) {
  struct mg_mqtt_message *msg = (struct mg_mqtt_message *) p;
  (void) nc;

  //if (ev != MG_EV_POLL) ESP_LOGI(TAG, "USER HANDLER GOT EVENT %d", ev);

  switch (ev) {
    case MG_EV_CONNECT: {
      struct mg_send_mqtt_handshake_opts opts;
      memset(&opts, 0, sizeof(opts));
      opts.user_name = s_user_name;
      opts.password = s_password;

      mg_set_protocol_mqtt(nc);
      mg_send_mqtt_handshake_opt(nc, "dummy", opts);
      break;
    }
    case MG_EV_MQTT_CONNACK:
      if (msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED) {
        ESP_LOGE(TAG, "Got mqtt connection error: %d", msg->connack_ret_code);
        while(1) { vTaskDelay(1); }
      }
      s_topic_expr.topic = s_topic;
      ESP_LOGI(TAG, "Subscribing to '%s'", s_topic);
      mg_mqtt_subscribe(nc, &s_topic_expr, 1, 42);
      break;
    case MG_EV_MQTT_PUBACK:
      ESP_LOGI(TAG, "Message publishing acknowledged (msg_id: %d)", msg->message_id);
      break;
    case MG_EV_MQTT_SUBACK:
      ESP_LOGI(TAG, "Subscription acknowledged");
      break;
    case MG_EV_MQTT_PUBLISH: {
#if 0
        char hex[1024] = {0};
        mg_hexdump(nc->recv_mbuf.buf, msg->payload.len, hex, sizeof(hex));
        printf("Got incoming message %.*s:\n%s", (int)msg->topic.len, msg->topic.p, hex);
#else
      ESP_LOGI(TAG, "Got incoming message %.*s: %.*s", (int) msg->topic.len,
             msg->topic.p, (int) msg->payload.len, msg->payload.p);
#endif
      break;
    }
    case MG_EV_CLOSE:
      //ESP_LOGE(TAG, "Connection closed");
      break;
  }
}


void mqtt_subscriber(void *pvParameters)
{
    /* Starting Subscriber */
    struct mg_mgr mgr;
    struct mg_connection *mc;
    char server_address[32];
    ESP_LOGD(TAG, "self_address=[%s]", self_address);
    sprintf(server_address, "%s:1883", self_address);
    ESP_LOGI(TAG, "MQTT subscriber started on %s", server_address);

    mg_mgr_init(&mgr, NULL);

    // Connect address is x.x.x.x:1883
    // 0.0.0.0:1883 not work
    mc =  mg_connect(&mgr, server_address, client_handler);
    if (mc == NULL) {
        ESP_LOGE(TAG, "mg_connect(%s) failed", server_address);
        while(1) { vTaskDelay(1); }
    }

    /* Processing events */
    while (1) {
      mg_mgr_poll(&mgr, 1000);
    }
}
