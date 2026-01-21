/*
	MQTT Broker for ESP32

	This code is in the Public Domain (or CC0 licensed, at your option.)

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
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_vfs_fat.h"
#include "mdns.h"

#include "lwip/dns.h"

#include "mongoose.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
#define esp_vfs_fat_spiflash_mount esp_vfs_fat_spiflash_mount_rw_wl
#define esp_vfs_fat_spiflash_unmount esp_vfs_fat_spiflash_unmount_rw_wl
#endif

/* This project use WiFi configuration that you can set via 'make menuconfig'.

	 If you'd rather not, just change the below entries to strings with
	 the config you want - ie #define ESP_WIFI_SSID "mywifissid"
*/

#if CONFIG_ST_MODE
/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
#endif

/* The event group allows multiple bits for each event, but we only care about one event 
 * - are we connected to the AP with an IP? */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "MAIN";

char *MOUNT_POINT = "/root";

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT) ESP_LOGI(TAG, "WIFI_EVENT event_id=%"PRIi32, event_id);
	if (event_base == IP_EVENT) ESP_LOGI(TAG, "IP_EVENT event_id=%"PRIi32, event_id);

#if CONFIG_AP_MODE
	if (event_id == WIFI_EVENT_AP_STACONNECTED) {
		wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
		ESP_LOGI(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
	} else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
		wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
		ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d", MAC2STR(event->mac), event->aid);
	}
#endif

#if CONFIG_ST_MODE
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
			esp_wifi_connect();
			xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
			s_retry_num++;
			ESP_LOGI(TAG, "retry to connect to the AP");
		} else {
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		}
		ESP_LOGI(TAG,"connect to the AP fail");
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
#endif
}

#if CONFIG_AP_MODE
void wifi_init_softap()
{
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_ap();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	//ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
	//ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

	wifi_config_t wifi_config = {
		.ap = {
			.ssid = CONFIG_ESP_WIFI_AP_SSID,
			.ssid_len = strlen(CONFIG_ESP_WIFI_AP_SSID),
			.password = CONFIG_ESP_WIFI_AP_PASSWORD,
			.max_connection = CONFIG_ESP_MAX_STA_CONN,
			.authmode = WIFI_AUTH_WPA_WPA2_PSK
		},
	};
	if (strlen(CONFIG_ESP_WIFI_AP_PASSWORD) == 0) {
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	}

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s", CONFIG_ESP_WIFI_AP_SSID, CONFIG_ESP_WIFI_AP_PASSWORD);
}
#endif // CONFIG_AP_MODE

#if CONFIG_ST_MODE

#if CONFIG_STATIC_IP
static esp_err_t example_set_dns_server(esp_netif_t *netif, uint32_t addr, esp_netif_dns_type_t type)
{
	if (addr && (addr != IPADDR_NONE)) {
		esp_netif_dns_info_t dns;
		dns.ip.u_addr.ip4.addr = addr;
		dns.ip.type = IPADDR_TYPE_V4;
		ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, type, &dns));
	}
	return ESP_OK;
}
#endif

esp_err_t wifi_init_sta()
{
	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_t *netif = esp_netif_create_default_wifi_sta();
	assert(netif);

#if CONFIG_STATIC_IP

	ESP_LOGI(TAG, "CONFIG_STATIC_IP_ADDRESS=[%s]",CONFIG_STATIC_IP_ADDRESS);
	ESP_LOGI(TAG, "CONFIG_STATIC_GW_ADDRESS=[%s]",CONFIG_STATIC_GW_ADDRESS);
	ESP_LOGI(TAG, "CONFIG_STATIC_NM_ADDRESS=[%s]",CONFIG_STATIC_NM_ADDRESS);

	/* Stop DHCP client */
	ESP_ERROR_CHECK(esp_netif_dhcpc_stop(netif));
	ESP_LOGI(TAG, "Stop DHCP Services");

	/* Set STATIC IP Address */
	esp_netif_ip_info_t ip_info;
	memset(&ip_info, 0 , sizeof(esp_netif_ip_info_t));
	ip_info.ip.addr = ipaddr_addr(CONFIG_STATIC_IP_ADDRESS);
	ip_info.netmask.addr = ipaddr_addr(CONFIG_STATIC_NM_ADDRESS);
	ip_info.gw.addr = ipaddr_addr(CONFIG_STATIC_GW_ADDRESS);;
	ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip_info));

	/* Set DNS Server */
	ESP_ERROR_CHECK(example_set_dns_server(netif, ipaddr_addr("8.8.8.8"), ESP_NETIF_DNS_MAIN));
	ESP_ERROR_CHECK(example_set_dns_server(netif, ipaddr_addr("8.8.4.4"), ESP_NETIF_DNS_BACKUP));

#endif // CONFIG_STATIC_IP

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
		ESP_EVENT_ANY_ID,
		&wifi_event_handler,
		NULL,
		&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
		IP_EVENT_STA_GOT_IP,
		&wifi_event_handler,
		NULL,
		&instance_got_ip));

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = CONFIG_ESP_WIFI_ST_SSID,
			.password = CONFIG_ESP_WIFI_ST_PASSWORD,
			/* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
			 * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
			 * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
			 * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
			 */
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,

			.pmf_cfg = {
				.capable = true,
				.required = false
			},
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
	 * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
		WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
		pdFALSE,
		pdFALSE,
		portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	 * happened. */
	esp_err_t ret_value = ESP_OK;
	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", CONFIG_ESP_WIFI_ST_SSID, CONFIG_ESP_WIFI_ST_PASSWORD);
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGE(TAG, "Failed to connect to SSID:%s, password:%s", CONFIG_ESP_WIFI_ST_SSID, CONFIG_ESP_WIFI_ST_PASSWORD);
		ret_value = ESP_FAIL;
	} else {
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
		ret_value = ESP_ERR_INVALID_STATE;
	}

	/* The event will not be processed after unregister */
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
	vEventGroupDelete(s_wifi_event_group);
	ESP_LOGI(TAG, "wifi_init_sta finished.");
	return ret_value;
}

void initialise_mdns(void)
{
	//initialize mDNS
	ESP_ERROR_CHECK( mdns_init() );
	//set mDNS hostname (required if you want to advertise services)
	ESP_ERROR_CHECK( mdns_hostname_set(CONFIG_MDNS_HOSTNAME) );
	ESP_LOGI(TAG, "mdns hostname set to: [%s]", CONFIG_MDNS_HOSTNAME);

	//initialize service
	ESP_ERROR_CHECK( mdns_service_add(NULL, "_mqtt", "_tcp", 1883, NULL, 0) );

#if 0
	//set default mDNS instance name
	ESP_ERROR_CHECK( mdns_instance_name_set("ESP32 with mDNS") );
#endif
}
#endif // CONFIG_ST_MODE

void mqtt_server(void *pvParameters);
void http_server(void *pvParameters);
void mqtt_subscriber(void *pvParameters);
void mqtt_publisher(void *pvParameters);

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
	esp_netif_ip_info_t ip_info;
	ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info));
	ESP_LOGI(TAG, "ESP32 is AP MODE");
#endif

#if CONFIG_ST_MODE
	ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
	wifi_init_sta();
	initialise_mdns();
	esp_netif_ip_info_t ip_info;
	ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info));
	ESP_LOGI(TAG, "ESP32 is STA MODE");
#endif

	/* Print the local IP address */
	//ESP_LOGI(TAG, "IP Address : %s", ip4addr_ntoa(&ip_info.ip));
	//ESP_LOGI(TAG, "Subnet mask: %s", ip4addr_ntoa(&ip_info.netmask));
	//ESP_LOGI(TAG, "Gateway		: %s", ip4addr_ntoa(&ip_info.gw));
	ESP_LOGI(TAG, "IP Address : " IPSTR, IP2STR(&ip_info.ip));
	ESP_LOGI(TAG, "Subnet Mask: " IPSTR, IP2STR(&ip_info.netmask));
	ESP_LOGI(TAG, "Gateway    : " IPSTR, IP2STR(&ip_info.gw));

	/* Start MQTT Server using tcp transport */
	//ESP_LOGI(TAG, "MQTT broker started on %s using Mongoose v%s", ip4addr_ntoa(&ip_info.ip), MG_VERSION);
	ESP_LOGI(TAG, "MQTT broker started on " IPSTR " using Mongoose v%s", IP2STR(&ip_info.ip), MG_VERSION);
	xTaskCreate(mqtt_server, "BROKER", 1024*4, NULL, 2, NULL);
	vTaskDelay(10);	// You need to wait until the task launch is complete.

#if CONFIG_SUBSCRIBE
	/* Start Subscriber */
	char cparam1[64];
	//sprintf(cparam1, "mqtt://%s:1883", ip4addr_ntoa(&ip_info.ip));
	sprintf(cparam1, "mqtt://" IPSTR ":1883", IP2STR(&ip_info.ip));
	xTaskCreate(mqtt_subscriber, "SUBSCRIBE", 1024*4, (void *)cparam1, 2, NULL);
	vTaskDelay(10);	// You need to wait until the task launch is complete.
#endif

#if CONFIG_PUBLISH
	/* Start Publisher */
	char cparam2[64];
	//sprintf(cparam2, "mqtt://%s:1883", ip4addr_ntoa(&ip_info.ip));
	sprintf(cparam2, "mqtt://" IPSTR ":1883", IP2STR(&ip_info.ip));
	xTaskCreate(mqtt_publisher, "PUBLISH", 1024*4, (void *)cparam2, 2, NULL);
	vTaskDelay(10);	// You need to wait until the task launch is complete.
#endif
}
