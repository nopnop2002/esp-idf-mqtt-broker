/* HTTP server using mongoose

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

#include "nvs_flash.h"
#include "esp_vfs_fat.h"

#include "mongoose.h"
#include "mqtt_server.h"

//static const char *s_root_dir = "/root";
//static const char *s_ssi_pattern = "#.shtml";

extern char *MOUNT_POINT;

// A list of client, held in memory
extern struct client *s_clients;

// A list of subscription, held in memory
extern struct sub *s_subs;

// A list of will topic & message, held in memory
extern struct will *s_wills;


esp_err_t makeIndexFile(char * filePath) {
	FILE *f = fopen(filePath, "wb");
	if (f == NULL) {
		ESP_LOGE(pcTaskGetName(NULL), "Failed to open file for writing");
		return ESP_FAIL;
	}
	fprintf(f, "<!DOCTYPE html>\n");
	fprintf(f, "<html lang=\"en\">\n");
	fprintf(f, "<head>\n");
	fprintf(f, "<title>MQTT</title>\n");
	fprintf(f, "</head>\n");
	fprintf(f, "<body>\n");
	fprintf(f, "MQTT Connection<br>\n");
	fprintf(f, "<table border=\"1\">\n");
	fprintf(f, "<tr style=\"color:#ffffff;\" bgcolor=\"#800000\">\n");
	fprintf(f, "<th>id</th>\n");
	fprintf(f, "<th>client-id</th>\n");
	fprintf(f, "<th>will-topic</th>\n");
	fprintf(f, "<th>will-payload</th>\n");
	fprintf(f, "<th>subscribe-topic</th>\n");
	fprintf(f, "</tr>\n");
	for (struct client *client = s_clients; client != NULL; client = client->next) {
		fprintf(f, "<tr bgcolor=\"#c0ffc0\">\n");
		fprintf(f, "<td>%p</td><td>%.*s</td><td><br></td><td><br></td><td><br></td>\n", client->c->fd, (int) client->cid.len, client->cid.ptr);
		fprintf(f, "</tr>\n");
		ESP_LOGI(pcTaskGetName(NULL), "CLIENT(ALL) %p [%.*s]", client->c->fd, (int) client->cid.len, client->cid.ptr);

		for (struct will *will = s_wills; will != NULL; will = will->next) {
			if (client->c != will->c) continue;
			fprintf(f, "<tr bgcolor=\"#ffffc0\">\n");
			fprintf(f, "<td><br></td><td><br></td><td>%.*s</td><td>%.*s</td><td><br></td>\n",
				(int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr);
			fprintf(f, "</tr>\n");
			ESP_LOGI(pcTaskGetName(NULL), "WILL(ALL) %p [%.*s] [%.*s] %d %d",
				will->c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
		}

		for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
			if (client->c != sub->c) continue;
			fprintf(f, "<tr bgcolor=\"#ffffc0\">\n");
			fprintf(f, "<td><br></td><td><br></td><td><br></td><td><br></td><td>%.*s</td>\n", (int) sub->topic.len, sub->topic.ptr);
			fprintf(f, "</tr>\n");
			ESP_LOGI(pcTaskGetName(NULL), "SUB(ALL) %p [%.*s]", sub->c->fd, (int) sub->topic.len, sub->topic.ptr);
		}
	}
	fprintf(f, "</table>\n");
	fprintf(f, "</body>\n");
	fprintf(f, "</html>\n");
	fclose(f);
	ESP_LOGD(pcTaskGetName(NULL), "File written");

#if 0
	for (struct client *client = s_clients; client != NULL; client = client->next) {
		ESP_LOGI(pcTaskGetName(NULL), "CLIENT(ALL) %p [%.*s]", client->c->fd, (int) client->cid.len, client->cid.ptr);
	}
#endif
	return ESP_OK;
}

// Event handler function
static void cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  if (ev == MG_EV_HTTP_MSG) {
	ESP_LOGI(pcTaskGetName(NULL), "MG_EV_HTTP_MSG");
	struct mg_http_message *hm = ev_data;
	ESP_LOGI(pcTaskGetName(NULL), "hm->uri.ptr=[%.*s] shm->uri.len=%d", hm->uri.len, hm->uri.ptr, hm->uri.len);
	if (mg_http_match_uri(hm, "/")) {
	  //mg_http_reply(c, 200, "", "{\"ram\": %lu}\n", xPortGetFreeHeapSize());

	  char indexFilePath[64];
	  sprintf(indexFilePath, "%s/index.htm", MOUNT_POINT);
	  ESP_LOGI(pcTaskGetName(NULL), "indexFilePath=[%s]", indexFilePath);
	  makeIndexFile(indexFilePath);
	  //mg_http_serve_file(c, hm, "/root/index.htm", "text/html; charset=utf-8", NULL);
	  mg_http_serve_file(c, hm, indexFilePath, "text/html; charset=utf-8", NULL);

#if 0
	} else {
	  struct mg_http_serve_opts opts = {s_root_dir, s_ssi_pattern};
	  mg_http_serve_dir(c, ev_data, &opts);
#endif
	}
  }
  (void) fn_data;
}

void http_server(void *pvParameters)
{
	char *task_parameter = (char *)pvParameters;
	ESP_LOGI(pcTaskGetName(NULL), "Start task_parameter=%s", task_parameter);
	char listening_address[64];
	strcpy(listening_address, task_parameter);
	//ESP_LOGI(pcTaskGetName(NULL), "started on %s", listening_address);

	struct mg_mgr mgr;
	struct mg_connection *mgc;
	mg_log_set("1"); // Set to log level to LL_ERROR
	//mg_log_set("3"); // Set to log level to LL_DEBUG
	mg_mgr_init(&mgr);

	if ((mgc = mg_http_listen(&mgr, listening_address, cb, &mgr)) == NULL) {	// Create WEB listener
		ESP_LOGE(pcTaskGetName(NULL), "Cannot listen on %s. Use http://ADDR:PORT or :PORT", listening_address);
		while(1) { vTaskDelay(1); }
	}

	char indexFilePath[64];
	sprintf(indexFilePath, "%s/index.htm", MOUNT_POINT);
	ESP_LOGI(pcTaskGetName(NULL), "indexFilePath=[%s]", indexFilePath);
	makeIndexFile(indexFilePath);

	/* Processing events */
	while (1) {
		mg_mgr_poll(&mgr, 0);
		vTaskDelay(1);
	}

	// Never reach here
	ESP_LOGI(pcTaskGetName(NULL), "finish");
	mg_mgr_free(&mgr);
}

