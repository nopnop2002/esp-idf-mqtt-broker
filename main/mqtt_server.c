/* MQTT Broker using mongoose

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
#include "mqtt_server.h"

static const char *s_listen_on = "mqtt://0.0.0.0:1883";

// A list of client, held in memory
struct client *s_clients = NULL;

// A list of subscription, held in memory
struct sub *s_subs = NULL;

// A list of will topic & message, held in memory
struct will *s_wills = NULL;

// Since version 7.8, mg_mqtt_next_sub() and mg_mqtt_next_unsub() are no longer supported.
static size_t mg_mqtt_next_topic(struct mg_mqtt_message *msg,
                                 struct mg_str *topic, uint8_t *qos,
                                 size_t pos) {
  unsigned char *buf = (unsigned char *) msg->dgram.ptr + pos;
  size_t new_pos;
  if (pos >= msg->dgram.len) return 0;

  topic->len = (size_t) (((unsigned) buf[0]) << 8 | buf[1]);
  topic->ptr = (char *) buf + 2;
  new_pos = pos + 2 + topic->len + (qos == NULL ? 0 : 1);
  if ((size_t) new_pos > msg->dgram.len) return 0;
  if (qos != NULL) *qos = buf[2 + topic->len];
  return new_pos;
}

size_t mg_mqtt_next_sub(struct mg_mqtt_message *msg, struct mg_str *topic,
                        uint8_t *qos, size_t pos) {
  uint8_t tmp;
  return mg_mqtt_next_topic(msg, topic, qos == NULL ? &tmp : qos, pos);
}

size_t mg_mqtt_next_unsub(struct mg_mqtt_message *msg, struct mg_str *topic,
                          size_t pos) {
  return mg_mqtt_next_topic(msg, topic, NULL, pos);
}

// Wildcard(#/+) support version
int _mg_strcmp(const struct mg_str str1, const struct mg_str str2) {
	size_t i1 = 0;
	size_t i2 = 0;
	while (i1 < str1.len && i2 < str2.len) {
		int c1 = str1.ptr[i1];
		int c2 = str2.ptr[i2];
		//printf("c1=%c c2=%c\n",c1, c2);

		// str2=[/hoge/#]
		if (c2 == '#') return 0;

		// str2=[/hoge/+/123]
		// Search next slash
		if (c2 == '+') {
			// str1=[/hoge//123]
			// str2=[/hoge/+/123]
			if (c1 == '/') {
				i2++;
			// str1=[/hoge/123/123]
			// str2=[/hoge/+/123]
			} else {
				for (i1=i1;i1+1<str1.len;i1++) {
					int c3 = str1.ptr[i1+1];
					//printf("i1=%ld c3=%c\n", i1, c3);
					if (c3 == '/') break;
				}
				i1++;
				i2++;
			}
		} else {
			if (c1 < c2) return -1;
			if (c1 > c2) return 1;
			i1++;
			i2++;
		}
	}
	if (i1 < str1.len) return 1;
	if (i2 < str2.len) return -1;
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

int _mg_mqtt_parse_header(struct mg_mqtt_message *msg, struct mg_str *client, struct mg_str *topic, 
		struct mg_str *payload, uint8_t *qos, uint8_t *retain) {
	client->len = 0;
	topic->len = 0;
	payload->len = 0;
	unsigned char *buf = (unsigned char *) msg->dgram.ptr;
	int Protocol_Name_length =	buf[2] << 8 | buf[3];
	int Connect_Flags_position = Protocol_Name_length + 5;
	uint8_t Connect_Flags = buf[Connect_Flags_position];
	ESP_LOGD("_mg_mqtt_parse_header", "Connect_Flags=%x", Connect_Flags);
	uint8_t Will_Flag = (Connect_Flags & WILL_FLAG) >> 2;
	*qos = (Connect_Flags & WILL_QOS) >> 3;
	*retain = (Connect_Flags & WILL_RETAIN) >> 5;
	ESP_LOGD("_mg_mqtt_parse_header", "Will_Flag=%d *qos=%x *retain=%x", Will_Flag, *qos, *retain);
	client->len = buf[Connect_Flags_position+3] << 8 | buf[Connect_Flags_position+4];
	client->ptr = (char *)&buf[Connect_Flags_position+5];
	ESP_LOGD("_mg_mqtt_parse_header", "client->len=%d", client->len);
	if (Will_Flag == 0) return 0;

#if 0
	int Client_Identifier_length = buf[Connect_Flags_position+3] << 8 | buf[Connect_Flags_position+4];
	ESP_LOGD("_mg_mqtt_parse_header", "Client_Identifier_length=%d", Client_Identifier_length);
	int Will_Topic_position = Protocol_Name_length + Client_Identifier_length + 10;
#endif

	int Will_Topic_position = Protocol_Name_length + client->len + 10;
	topic->len = buf[Will_Topic_position] << 8 | buf[Will_Topic_position+1];
	topic->ptr = (char *)&(buf[Will_Topic_position]) + 2;
	ESP_LOGD("_mg_mqtt_parse_header", "topic->len=%d topic->ptr=[%.*s]", topic->len, topic->len, topic->ptr);
	int Will_Payload_position = Will_Topic_position + topic->len + 2;
	payload->len = buf[Will_Payload_position] << 8 | buf[Will_Payload_position+1];
	payload->ptr = (char *)&(buf[Will_Payload_position]) + 2;
	ESP_LOGD("_mg_mqtt_parse_header", "payload->len=%d payload->ptr=[%.*s]", payload->len, payload->len, payload->ptr);
	return 1;
}

int _mg_mqtt_status() {
	for (struct client *client = s_clients; client != NULL; client = client->next) {
		ESP_LOGI(pcTaskGetName(NULL), "CLIENT(ALL) %p [%.*s]", client->c->fd, (int) client->cid.len, client->cid.ptr);
		for (struct will *will = s_wills; will != NULL; will = will->next) {
			if (client->c != will->c) continue;
			ESP_LOGI(pcTaskGetName(NULL), "WILL(ALL) %p [%.*s] [%.*s] %d %d", 
			will->c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
		}
		for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
			if (client->c != sub->c) continue;
			ESP_LOGI(pcTaskGetName(NULL), "SUB(ALL) %p [%.*s]", sub->c->fd, (int) sub->topic.len, sub->topic.ptr);
		}
	}

#if 0
	for (struct will *will = s_wills; will != NULL; will = will->next) {
		ESP_LOGI(pcTaskGetName(NULL), "WILL(ALL) %p [%.*s] [%.*s] %d %d", 
		will->c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
	}
#endif

#if 0
	for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
		ESP_LOGI(pcTaskGetName(NULL), "SUB(ALL) %p [%.*s]", sub->c->fd, (int) sub->topic.len, sub->topic.ptr);
	}
#endif

	return 0;
}

int isasciis(char * buffer, int length) {
	for (int i=0;i<length;i++) {
		if (!isascii(buffer[i])) return 0;
	}
	return 1;

}

// Event handler function
static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
	if (ev == MG_EV_MQTT_CMD) {
		struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
		ESP_LOGD(pcTaskGetName(NULL),"cmd %d qos %d", mm->cmd, mm->qos);
		switch (mm->cmd) {
			case MQTT_CMD_CONNECT: {
				ESP_LOGI(pcTaskGetName(NULL), "CONNECT");
				ESP_LOGD(pcTaskGetName(NULL),"total_size(MALLOC_CAP_8BIT):%d", heap_caps_get_total_size(MALLOC_CAP_8BIT));
				ESP_LOGD(pcTaskGetName(NULL),"total_size(MALLOC_CAP_32BIT):%d", heap_caps_get_total_size(MALLOC_CAP_32BIT));
				ESP_LOGD(pcTaskGetName(NULL),"free_size(MALLOC_CAP_8BIT):%d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
				ESP_LOGD(pcTaskGetName(NULL),"free_size(MALLOC_CAP_32BIT):%d", heap_caps_get_free_size(MALLOC_CAP_32BIT));

				// Parse the header to retrieve will information.
				//_mg_mqtt_dump("CONNECT", mm);
				struct mg_str cid;
				struct mg_str topic;
				struct mg_str payload;
				uint8_t qos;
				uint8_t retain;
				int willFlag = _mg_mqtt_parse_header(mm, &cid, &topic, &payload, &qos, &retain);
				ESP_LOGI(pcTaskGetName(NULL), "cid=[%.*s] willFlag=%d", cid.len, cid.ptr, willFlag);

				// Set tcp socket keepalive options
				// timeout = keepidle+(keepcnt*keepintvl)
				// timeout = 60+(1*10)=70
				int keepAlive = 1;
				setsockopt( (int) c->fd, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
				int keepIdle = 60; // default is 7200 Sec
				setsockopt( (int) c->fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
				int keepInterval = 10; // default is 75 Sec
				setsockopt( (int) c->fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
				int keepCount = 1; // default is 9 count
				setsockopt( (int) c->fd, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

#if 0
				// Client connects. Add to the client-id list
				struct client *client = calloc(1, sizeof(*client));
				client->c = c;
				client->cid = mg_strdup(cid);
				LIST_ADD_HEAD(struct client, &s_clients, client);
				ESP_LOGD(pcTaskGetName(NULL), "CLIENT ADD %p [%.*s]", c->fd, (int) client->cid.len, client->cid.ptr);
				ESP_LOGI(pcTaskGetName(NULL), "CLIENT ADD %p", client);
#endif

				// Client connects. Add to the will list
				if (willFlag == 1) {
					struct will *will = calloc(1, sizeof(*will));
					will->c = c;
					will->topic = mg_strdup(topic);
					will->payload = mg_strdup(payload);
					will->qos = qos;
					will->retain = retain;
					LIST_ADD_HEAD(struct will, &s_wills, will);
					ESP_LOGD(pcTaskGetName(NULL), "WILL ADD %p [%.*s] [%.*s] %d %d", 
					c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
				}
				_mg_mqtt_status();

				// Client connects. Return success, do not check user/password
				uint8_t response[] = {0, 0};
				mg_mqtt_send_header(c, MQTT_CMD_CONNACK, 0, sizeof(response));
				mg_send(c, response, sizeof(response));
				break;
			}
			case MQTT_CMD_SUBSCRIBE: {
				// Client subscribe
				ESP_LOGI(pcTaskGetName(NULL), "MQTT_CMD_SUBSCRIBE");
				int pos = 4;	// Initial topic offset, where ID ends
				uint8_t qos, resp[256];
				struct mg_str topic;
				int num_topics = 0;
				while ((pos = mg_mqtt_next_sub(mm, &topic, &qos, pos)) > 0) {
					struct sub *sub = calloc(1, sizeof(*sub));
					sub->c = c;
					sub->topic = mg_strdup(topic);
					sub->qos = qos;
					LIST_ADD_HEAD(struct sub, &s_subs, sub);
					ESP_LOGI(pcTaskGetName(NULL), "SUB ADD %p [%.*s]", c->fd, (int) sub->topic.len, sub->topic.ptr);
					resp[num_topics++] = qos;
				}
				mg_mqtt_send_header(c, MQTT_CMD_SUBACK, 0, num_topics + 2);
				uint16_t id = mg_htons(mm->id);
				mg_send(c, &id, 2);
				mg_send(c, resp, num_topics);
				_mg_mqtt_status();
				break;
			}
			case MQTT_CMD_UNSUBSCRIBE: {
				// Client unsubscribes. Remove from the subscription list
				ESP_LOGI(pcTaskGetName(NULL), "MQTT_CMD_UNSUBSCRIBE");
				//_mg_mqtt_dump("UNSUBSCRIBE", mm);
				int pos = 4;	// Initial topic offset, where ID ends
				struct mg_str topic;
				while ((pos = mg_mqtt_next_unsub(mm, &topic, pos)) > 0) {
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
						ESP_LOGI(pcTaskGetName(NULL), "DELETE SUB %p [%.*s]", c->fd, (int) sub->topic.len, sub->topic.ptr);
						free((void *)sub->topic.ptr);
						LIST_DELETE(struct sub, &s_subs, sub);
						free(sub);
					}
					for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
						ESP_LOGI(pcTaskGetName(NULL), "SUB[a] %p [%.*s]", sub->c->fd, (int) sub->topic.len, sub->topic.ptr);
					}
				}
				_mg_mqtt_status();
				break;
			}
			case MQTT_CMD_PUBLISH: {
				// Client published message. Push to all subscribed channels
				//ESP_LOGI(pcTaskGetName(NULL), "mm->data.ptr[0]=0x%x", mm->data.ptr[0]);
				//if (isascii(mm->data.ptr[0])) {
				// Make sure all characters are ASCII codes
				if (isasciis((char *)mm->data.ptr, mm->topic.len)) {
					ESP_LOGI(pcTaskGetName(NULL), "PUB %p [%.*s] -> [%.*s]", c->fd, (int) mm->data.len,
						mm->data.ptr, (int) mm->topic.len, mm->topic.ptr);
				} else {
					ESP_LOGI(pcTaskGetName(NULL), "PUB %p [BINARY] -> [%.*s]", c->fd,
						(int) mm->topic.len, mm->topic.ptr);
				}
				for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
					if (_mg_strcmp(mm->topic, sub->topic) != 0) continue;
					//mg_mqtt_pub(sub->c, &mm->topic, &mm->data);
					//mg_mqtt_pub(sub->c, &mm->topic, &mm->data, 1, false);
					//for Ver7.6
					mg_mqtt_pub(sub->c, mm->topic, mm->data, 1, false);
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

		ESP_LOGD(pcTaskGetName(NULL),"total_size(MALLOC_CAP_8BIT):%d", heap_caps_get_total_size(MALLOC_CAP_8BIT));
		ESP_LOGD(pcTaskGetName(NULL),"total_size(MALLOC_CAP_32BIT):%d", heap_caps_get_total_size(MALLOC_CAP_32BIT));
		ESP_LOGI(pcTaskGetName(NULL),"free_size(MALLOC_CAP_8BIT):%d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
		ESP_LOGI(pcTaskGetName(NULL),"free_size(MALLOC_CAP_32BIT):%d", heap_caps_get_free_size(MALLOC_CAP_32BIT));

		// Client disconnects. Remove from the client-id list
		for (struct client *client = s_clients; client != NULL; client = client->next) {
			ESP_LOGD(pcTaskGetName(NULL), "CLIENT(b) %p [%.*s]", client->c->fd, (int) client->cid.len, client->cid.ptr);
		}
		for (struct client *next, *client = s_clients; client != NULL; client = next) {
			next = client->next;
			ESP_LOGD(pcTaskGetName(NULL), "c->fd=%p client->c->fd=%p", c->fd, client->c->fd);
			if (c != client->c) continue;
			ESP_LOGD(pcTaskGetName(NULL), "CLIENT DEL %p [%.*s]", c->fd, (int) client->cid.len, client->cid.ptr);
			ESP_LOGI(pcTaskGetName(NULL), "CLIENT DEL %p", client);
			free((void *)client->cid.ptr);
			LIST_DELETE(struct client, &s_clients, client);
			free(client);
		}
		for (struct client *client = s_clients; client != NULL; client = client->next) {
			ESP_LOGD(pcTaskGetName(NULL), "CLIENT(a) %p [%.*s]", client->c->fd, (int) client->cid.len, client->cid.ptr);
		}

		// Client disconnects. Remove from the subscription list
		for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
			ESP_LOGD(pcTaskGetName(NULL), "SUB[b] %p [%.*s]", sub->c->fd, (int) sub->topic.len, sub->topic.ptr);
		}
		for (struct sub *next, *sub = s_subs; sub != NULL; sub = next) {
			next = sub->next;
			ESP_LOGD(pcTaskGetName(NULL), "c->fd=%p sub->c->fd=%p", c->fd, sub->c->fd);
			if (c != sub->c) continue;
			ESP_LOGD(pcTaskGetName(NULL), "SUB DEL %p [%.*s]", c->fd, (int) sub->topic.len, sub->topic.ptr);
			free((void *)sub->topic.ptr);
			LIST_DELETE(struct sub, &s_subs, sub);
			free(sub);
		}

		// Judgment to send will
		for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
			ESP_LOGD(pcTaskGetName(NULL), "SUB[a] %p [%.*s]", sub->c->fd, (int) sub->topic.len, sub->topic.ptr);
			for (struct will *will = s_wills; will != NULL; will = will->next) {
				ESP_LOGD(pcTaskGetName(NULL), "WILL(ALL) %p [%.*s] [%.*s] %d %d", 
					will->c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
				//if (c == will->c) continue;
				if (sub->c == will->c) continue;
				ESP_LOGD(pcTaskGetName(NULL), "WILL(CMP) %p [%.*s] [%.*s] %d %d", 
					will->c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
				if (_mg_strcmp(will->topic, sub->topic) != 0) continue;
				//mg_mqtt_pub(sub->c, &will->topic, &will->payload);
				//mg_mqtt_pub(sub->c, &will->topic, &will->payload, 1, false);
				//for Ver7.6
				mg_mqtt_pub(sub->c, will->topic, will->payload, 1, false);
			}
		}

		// Client disconnects. Remove from the will list
		for (struct will *will = s_wills; will != NULL; will = will->next) {
			ESP_LOGD(pcTaskGetName(NULL), "WILL[b] %p [%.*s] [%.*s] %d %d", 
				will->c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
		}
		for (struct will *next, *will = s_wills; will != NULL; will = next) {
			next = will->next;
			ESP_LOGD(pcTaskGetName(NULL), "WILL %p [%.*s] [%.*s] %d %d", 
				c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
			if (c != will->c) continue;
			ESP_LOGD(pcTaskGetName(NULL), "WILL DEL %p [%.*s] [%.*s] %d %d", 
				c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
			free((void *)will->topic.ptr);
			LIST_DELETE(struct will, &s_wills, will);
			free(will);
		}
		for (struct will *will = s_wills; will != NULL; will = will->next) {
			ESP_LOGD(pcTaskGetName(NULL), "WILL[a] %p [%.*s] [%.*s] %d %d", 
				will->c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
		}
		_mg_mqtt_status();
	} // MG_EV_CLOSE
	(void) fn_data;
}

void mqtt_server(void *pvParameters)
{
	/* Starting Broker */
	ESP_LOGI(pcTaskGetName(NULL), "Start");
	struct mg_mgr mgr;
	mg_log_set(1); // Set to log level to LL_ERROR
	//mg_log_set(3); // Set to log level to LL_DEBUG
	mg_mgr_init(&mgr);
	mg_mqtt_listen(&mgr, s_listen_on, fn, NULL); // Create MQTT listener
	//ESP_LOGI(pcTaskGetName(NULL), "Starting Mongoose v%s MQTT Server", MG_VERSION);

	/* Processing events */
	while (1) {
		mg_mgr_poll(&mgr, 0);
		vTaskDelay(1);
	}

	// Never reach here
	ESP_LOGI(pcTaskGetName(NULL), "finish");
	mg_mgr_free(&mgr);
}

