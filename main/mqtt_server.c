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

// A list of will topic & message, held in memory
struct will {
  struct will *next;
  struct mg_connection *c;
  struct mg_str topic;
  struct mg_str payload;
  uint8_t qos;
  uint8_t retain;
};
static struct will *s_wills = NULL;

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

void _mg_mqtt_dump(char * tag, struct mg_mqtt_message *msg) {
	unsigned char *buf = (unsigned char *) msg->dgram.ptr;
	ESP_LOGI(pcTaskGetName(NULL),"%s=%x %x", tag, buf[0], buf[1]);
	int length = buf[1] + 2;
	ESP_LOG_BUFFER_HEXDUMP(tag, buf, length, ESP_LOG_INFO);
}

#define	WILL_FLAG	0x04
#define WILL_QOS	0x18
#define WILL_RETAIN	0x20

int _mg_mqtt_will_topic(struct mg_mqtt_message *msg, struct mg_str *topic, 
		struct mg_str *payload, uint8_t *qos, uint8_t *retain) {
	topic->len = 0;
	payload->len = 0;
	unsigned char *buf = (unsigned char *) msg->dgram.ptr;
	int Protocol_Name_length =	buf[2] << 8 | buf[3];
	int Connect_Flags_position = Protocol_Name_length + 5;
	uint8_t Connect_Flags = buf[Connect_Flags_position];
	ESP_LOGD("_mg_mqtt_will_topic", "Connect_Flags=%x", Connect_Flags);
	uint8_t Will_Flag = (Connect_Flags & WILL_FLAG) >> 2;
	*qos = (Connect_Flags & WILL_QOS) >> 3;
	*retain = (Connect_Flags & WILL_RETAIN) >> 5;
	ESP_LOGD("_mg_mqtt_will_topic", "Will_Flag=%d *qos=%x *retain=%x", Will_Flag, *qos, *retain);
	if (Will_Flag == 0) return 0;

	int Client_Identifier_length = buf[Connect_Flags_position+3] << 8 | buf[Connect_Flags_position+4];
	ESP_LOGD("_mg_mqtt_will_topic", "Client_Identifier_length=%d", Client_Identifier_length);
	int Will_Topic_position = Protocol_Name_length + Client_Identifier_length + 10;
	topic->len = buf[Will_Topic_position] << 8 | buf[Will_Topic_position+1];
	topic->ptr = (char *)&(buf[Will_Topic_position]) + 2;
	ESP_LOGI("_mg_mqtt_will_topic", "topic->len=%d topic->ptr=[%.*s]", topic->len, topic->len, topic->ptr);
	int Will_Payload_position = Will_Topic_position + topic->len + 2;
	payload->len = buf[Will_Payload_position] << 8 | buf[Will_Payload_position+1];
	payload->ptr = (char *)&(buf[Will_Payload_position]) + 2;
	ESP_LOGI("_mg_mqtt_will_topic", "payload->len=%d payload->ptr=[%.*s]", payload->len, payload->len, payload->ptr);
	return 1;
}

int _mg_mqtt_next_unsub(struct mg_mqtt_message *msg, struct mg_str *topic, int pos) {
	unsigned char *buf = (unsigned char *) msg->dgram.ptr + pos;
	int new_pos;
	if ((size_t) pos >= msg->dgram.len) return -1;

	topic->len = buf[0] << 8 | buf[1];
	topic->ptr = (char *) buf + 2;
	new_pos = pos + 2 + topic->len + 0;
	if ((size_t) new_pos > msg->dgram.len) return -1;
	//*qos = buf[2 + topic->len];
	return new_pos;
}

// Event handler function
static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  if (ev == MG_EV_MQTT_CMD) {
	struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
	ESP_LOGD(pcTaskGetName(NULL),"cmd %d qos %d", mm->cmd, mm->qos);
	switch (mm->cmd) {
	  case MQTT_CMD_CONNECT: {
		ESP_LOGI(pcTaskGetName(NULL), "CONNECT");

		// Parse the header to retrieve will information.
		//_mg_mqtt_dump("CONNECT", mm);
		struct mg_str topic;
		struct mg_str payload;
		uint8_t qos;
		uint8_t retain;
		int willFlag = _mg_mqtt_will_topic(mm, &topic, &payload, &qos, &retain);

		// Add Will topic to s_wills
		if (willFlag == 1) {
		  struct will *will = calloc(1, sizeof(*will));
		  will->c = c;
		  will->topic = mg_strdup(topic);
		  will->payload = mg_strdup(payload);
		  will->qos = qos;
		  will->retain = retain;
		  LIST_ADD_HEAD(struct will, &s_wills, will);
		  ESP_LOGI(pcTaskGetName(NULL), "WILL(ADD) %p [%.*s] [%.*s] %d %d", 
			c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
		}
		for (struct will *will = s_wills; will != NULL; will = will->next) {
			ESP_LOGI(pcTaskGetName(NULL), "WILL(ALL) %p [%.*s] [%.*s] %d %d", 
			will->c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
		}

		// Client connects. Return success, do not check user/password
		uint8_t response[] = {0, 0};
		mg_mqtt_send_header(c, MQTT_CMD_CONNACK, 0, sizeof(response));
		mg_send(c, response, sizeof(response));
		break;
	  }
	  case MQTT_CMD_SUBSCRIBE: {
		// Client subscribes
		ESP_LOGI(pcTaskGetName(NULL), "MQTT_CMD_SUBSCRIBE");
		//_mg_mqtt_dump("SUBSCRIBE", mm);
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

		  for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
			ESP_LOGI(pcTaskGetName(NULL), "SUB[a] %p [%.*s]", sub->c->fd, (int) sub->topic.len, sub->topic.ptr);
		  }
		}
		break;
	  }
	  case MQTT_CMD_UNSUBSCRIBE: {
		// Client unsubscribes
		ESP_LOGI(pcTaskGetName(NULL), "MQTT_CMD_UNSUBSCRIBE");
		//_mg_mqtt_dump("UNSUBSCRIBE", mm);
		int pos = 4;  // Initial topic offset, where ID ends
		struct mg_str topic;
		while ((pos = _mg_mqtt_next_unsub(mm, &topic, pos)) > 0) {
		  ESP_LOGI(pcTaskGetName(NULL), "UNSUB %p [%.*s]", c->fd, (int) topic.len, topic.ptr);
		  // Remove from the subscription list
		  for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
			ESP_LOGI(pcTaskGetName(NULL), "SUB[b] %p [%.*s]", sub->c->fd, (int) sub->topic.len, sub->topic.ptr);
		  }
		  for (struct sub *next, *sub = s_subs; sub != NULL; sub = next) {
			next = sub->next;
			ESP_LOGD(pcTaskGetName(NULL), "c->fd=%p sub->c->fd=%p", c->fd, sub->c->fd);
			if (c != sub->c) continue;
			if (strncmp(topic.ptr, sub->topic.ptr, topic.len) != 0) continue;
			ESP_LOGI(pcTaskGetName(NULL), "DELETE %p [%.*s]", c->fd, (int) sub->topic.len, sub->topic.ptr);
			LIST_DELETE(struct sub, &s_subs, sub);
  		  }
		  for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
			ESP_LOGI(pcTaskGetName(NULL), "SUB[a] %p [%.*s]", sub->c->fd, (int) sub->topic.len, sub->topic.ptr);
		  }
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
	ESP_LOGI(pcTaskGetName(NULL), "MG_EV_CLOSE %p", c->fd);
	// Client disconnects. Remove from the subscription list
	for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
		ESP_LOGI(pcTaskGetName(NULL), "SUB[b] %p [%.*s]", sub->c->fd, (int) sub->topic.len, sub->topic.ptr);
	}
	for (struct sub *next, *sub = s_subs; sub != NULL; sub = next) {
		next = sub->next;
		ESP_LOGD(pcTaskGetName(NULL), "c->fd=%p sub->c->fd=%p", c->fd, sub->c->fd);
		if (c != sub->c) continue;
		ESP_LOGI(pcTaskGetName(NULL), "DELETE %p [%.*s]", c->fd, (int) sub->topic.len, sub->topic.ptr);
		LIST_DELETE(struct sub, &s_subs, sub);
	}

	// Judgment to send will
	for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
		ESP_LOGI(pcTaskGetName(NULL), "SUB[a] %p [%.*s]", sub->c->fd, (int) sub->topic.len, sub->topic.ptr);
		for (struct will *will = s_wills; will != NULL; will = will->next) {
			ESP_LOGI(pcTaskGetName(NULL), "WILL(ALL) %p [%.*s] [%.*s] %d %d", 
				will->c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
			//if (c == will->c) continue;
			if (sub->c == will->c) continue;
			ESP_LOGI(pcTaskGetName(NULL), "WILL(CMP) %p [%.*s] [%.*s] %d %d", 
				will->c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
			if (_mg_strcmp(will->topic, sub->topic) != 0) continue;
			mg_mqtt_pub(sub->c, &will->topic, &will->payload);
		}
	}

	// Client disconnects. Remove from the will list
	for (struct will *will = s_wills; will != NULL; will = will->next) {
		ESP_LOGI(pcTaskGetName(NULL), "WILL[b] %p [%.*s] [%.*s] %d %d", 
			will->c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
	}
	for (struct will *next, *will = s_wills; will != NULL; will = next) {
		next = will->next;
		ESP_LOGD(pcTaskGetName(NULL), "WILL %p [%.*s] [%.*s] %d %d", 
			c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
		if (c != will->c) continue;
		ESP_LOGI(pcTaskGetName(NULL), "DELETE WILL %p [%.*s] [%.*s] %d %d", 
			c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
		LIST_DELETE(struct will, &s_wills, will);
	}
	for (struct will *will = s_wills; will != NULL; will = will->next) {
		ESP_LOGI(pcTaskGetName(NULL), "WILL[a] %p [%.*s] [%.*s] %d %d", 
			will->c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
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

