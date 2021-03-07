/* MQTT Broker using tcp transport

   This code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"

#include "mongoose.h"

static const char *s_listen_on = "mqtt://0.0.0.0:1883";

// A list of subscription, held in memory
struct sub {
  struct sub *next;
  struct mg_connection *c;
  struct mg_str topic;
  uint8_t qos;
};
static struct sub *s_subs = NULL;


// Wildcard support version
int _mg_strcmp(const struct mg_str str1, const struct mg_str str2) {
  size_t i = 0;
  while (i < str1.len && i < str2.len) {
	int c1 = str1.ptr[i];
	int c2 = str2.ptr[i];
	//printf("c2=%x\n",c2);
	if (c2 == '#') return 0;
	if (c1 < c2) return -1;
	if (c1 > c2) return 1;
	i++;
  }
  if (i < str1.len) return 1;
  if (i < str2.len) return -1;
  return 0;
}

// Event handler function
static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  if (ev == MG_EV_MQTT_CMD) {
	struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
	ESP_LOGD(pcTaskGetName(NULL),"cmd %d qos %d", mm->cmd, mm->qos);
	switch (mm->cmd) {
	  case MQTT_CMD_CONNECT: {
		ESP_LOGI(pcTaskGetName(NULL), "CONNECT");
		// Client connects. Return success, do not check user/password
		uint8_t response[] = {0, 0};
		mg_mqtt_send_header(c, MQTT_CMD_CONNACK, 0, sizeof(response));
		mg_send(c, response, sizeof(response));
		break;
	  }
	  case MQTT_CMD_SUBSCRIBE: {
		// Client subscribes
		int pos = 4;  // Initial topic offset, where ID ends
		uint8_t qos;
		struct mg_str topic;
		while ((pos = mg_mqtt_next_sub(mm, &topic, &qos, pos)) > 0) {
		  struct sub *sub = calloc(1, sizeof(*sub));
		  sub->c = c;
		  sub->topic = mg_strdup(topic);
		  sub->qos = qos;
		  LIST_ADD_HEAD(struct sub, &s_subs, sub);
		  ESP_LOGI(pcTaskGetName(NULL), "SUB %p [%.*s]", c->fd, (int) sub->topic.len, sub->topic.ptr);
		}
		break;
	  }
	  case MQTT_CMD_PUBLISH: {
		// Client published message. Push to all subscribed channels
		ESP_LOGI(pcTaskGetName(NULL), "PUB %p [%.*s] -> [%.*s]", c->fd, (int) mm->data.len,
					  mm->data.ptr, (int) mm->topic.len, mm->topic.ptr);
		for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
		  //if (mg_strcmp(mm->topic, sub->topic) != 0) continue;
		  if (_mg_strcmp(mm->topic, sub->topic) != 0) continue;
		  mg_mqtt_pub(sub->c, &mm->topic, &mm->data);
		}
		break;
	  }
	  case MQTT_CMD_PINGREQ: {
		ESP_LOGI(pcTaskGetName(NULL), "PINGREQ %p", c->fd);
        mg_mqtt_pong(c); // Send PINGRESP
		break;
      }
	}
  } else if (ev == MG_EV_CLOSE) {
	// Client disconnects. Remove from the subscription list
	for (struct sub *next, *sub = s_subs; sub != NULL; sub = next) {
	  next = sub->next;
	  if (c != sub->c) continue;
	  ESP_LOGI(pcTaskGetName(NULL), "UNSUB %p [%.*s]", c->fd, (int) sub->topic.len, sub->topic.ptr);
	  LIST_DELETE(struct sub, &s_subs, sub);
	}
  }
  (void) fn_data;
}

void mqtt_server(void *pvParameters)
{
	/* Starting Broker */
	ESP_LOGI(pcTaskGetName(NULL), "start");
	struct mg_mgr mgr;
	//mg_log_set("3"); // Set to log level to LL_DEBUG
	mg_mgr_init(&mgr);
	mg_mqtt_listen(&mgr, s_listen_on, fn, NULL);  // Create MQTT listener

	/* Processing events */
	while (1) {
	  mg_mgr_poll(&mgr, 0);
	  vTaskDelay(1);
	}

	// Never reach here
	ESP_LOGI(pcTaskGetName(NULL), "finish");
	mg_mgr_free(&mgr);
}

