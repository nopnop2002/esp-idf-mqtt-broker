/* MQTT Broker Subscriber

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"

#include "mongoose.h"

#if CONFIG_SUBSCRIBE

static const char *sub_topic = "#";
static const char *will_topic = "WILL";

static EventGroupHandle_t s_wifi_event_group;
/* The event group allows multiple bits for each event, but we only care about one event
 * - are we connected to the MQTT? */
static int MQTT_CONNECTED_BIT = BIT0;

static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  if (ev == MG_EV_ERROR) {
	// On error, log error message
	ESP_LOGE(pcTaskGetName(NULL), "MG_EV_ERROR %p %s", c->fd, (char *) ev_data);
	xEventGroupClearBits(s_wifi_event_group, MQTT_CONNECTED_BIT);
  } else if (ev == MG_EV_CONNECT) {
	ESP_LOGI(pcTaskGetName(NULL), "MG_EV_CONNECT");
	// If target URL is SSL/TLS, command client connection to use TLS
	if (mg_url_is_ssl((char *)fn_data)) {
	  struct mg_tls_opts opts = {.ca = "ca.pem"};
	  mg_tls_init(c, &opts);
	}
  } else if (ev == MG_EV_MQTT_OPEN) {
	ESP_LOGI(pcTaskGetName(NULL), "MG_EV_OPEN");
	// MQTT connect is successful
	ESP_LOGI(pcTaskGetName(NULL), "CONNECTED to %s", (char *)fn_data);
	xEventGroupSetBits(s_wifi_event_group, MQTT_CONNECTED_BIT);

#if 0
	struct mg_str topic = mg_str(sub_topic);
	struct mg_str data = mg_str("hello");
	mg_mqtt_sub(c, &topic, 1);
	ESP_LOGI(pcTaskGetName(NULL), "SUBSCRIBED to %.*s", (int) topic.len, topic.ptr);
#endif

#if 0
	mg_mqtt_pub(c, &topic, &data);
	LOG(LL_INFO, ("PUBSLISHED %.*s -> %.*s", (int) data.len, data.ptr,
				  (int) topic.len, topic.ptr));
#endif

  } else if (ev == MG_EV_MQTT_MSG) {
	// When we get echo response, print it
	struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
	ESP_LOGI(pcTaskGetName(NULL), "RECEIVED %.*s <- %.*s", (int) mm->data.len, mm->data.ptr,
				  (int) mm->topic.len, mm->topic.ptr);
  }

#if 0
  if (ev == MG_EV_ERROR || ev == MG_EV_CLOSE || ev == MG_EV_MQTT_MSG) {
	*(bool *) fn_data = true;  // Signal that we're done
  }
#endif
}



void mqtt_subscriber(void *pvParameters)
{
	char *task_parameter = (char *)pvParameters;
	ESP_LOGD(pcTaskGetName(NULL), "Start task_parameter=%s", task_parameter);
	char url[64];
	strcpy(url, task_parameter);
	ESP_LOGI(pcTaskGetName(NULL), "started on %s", url);

	/* Starting Subscriber */
	struct mg_mgr mgr;
	struct mg_mqtt_opts opts;  // MQTT connection options
	//bool done = false;		 // Event handler flips it to true when done
	mg_mgr_init(&mgr);		   // Initialise event manager
	memset(&opts, 0, sizeof(opts));					// Set MQTT options
	//opts.client_id = mg_str("SUB");				// Set Client ID
	opts.client_id = mg_str(pcTaskGetName(NULL));	// Set Client ID
	//opts.qos = 1;									// Set QoS to 1
	//for Ver7.6
	opts.will_qos = 1;									// Set QoS to 1
	opts.will_topic = mg_str(will_topic);			// Set last will topic
	opts.will_message = mg_str("goodbye");			// And last will message

	// Connect address is x.x.x.x:1883
	// 0.0.0.0:1883 not work
	ESP_LOGD(pcTaskGetName(NULL), "url=[%s]", url);
	//static const char *url = "mqtt://broker.hivemq.com:1883";
	//mg_mqtt_connect(&mgr, url, &opts, fn, &done);  // Create client connection
	//mg_mqtt_connect(&mgr, url, &opts, fn, &done);  // Create client connection
	struct mg_connection *mgc;
	mgc = mg_mqtt_connect(&mgr, url, &opts, fn, &url);	// Create client connection

	/* Processing events */
	s_wifi_event_group = xEventGroupCreate();

	while (1) {
		EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
			MQTT_CONNECTED_BIT,
			pdTRUE,
			pdTRUE,
			0);
		ESP_LOGD(pcTaskGetName(NULL), "bits=0x%"PRIx32, bits);
		if ((bits & MQTT_CONNECTED_BIT) != 0) {
			struct mg_str topic = mg_str(sub_topic);
			//mg_mqtt_sub(mgc, &topic);
			//mg_mqtt_sub(mgc, &topic, 1);
			//for Ver7.6
			mg_mqtt_sub(mgc, topic, 1);
			ESP_LOGI(pcTaskGetName(NULL), "SUBSCRIBED to %.*s", (int) topic.len, topic.ptr);
		}
		mg_mgr_poll(&mgr, 0);
		vTaskDelay(1);
	}

	// Never reach here
	ESP_LOGI(pcTaskGetName(NULL), "finish");
	mg_mgr_free(&mgr);								// Finished, cleanup
}
#endif
