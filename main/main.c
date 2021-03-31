/* MQTT Broker for ESP32

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
#include "esp_vfs_fat.h"
#include "mdns.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "mongoose.h"

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
const int WIFI_CONNECTED_BIT = BIT0;

static const char *TAG = "MAIN";

char *MOUNT_POINT = "/root";

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
		if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
			esp_wifi_connect();
			xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
			s_retry_num++;
			ESP_LOGI(TAG, "retry to connect to the AP");
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
#if ESP_IDF_VERSION_MAJOR >= 4 && ESP_IDF_VERSION_MINOR >= 1
	ESP_LOGI(TAG,"ESP-IDF esp_netif");
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_ap();
#else
	ESP_LOGI(TAG,"ESP-IDF tcpip_adapter");
	tcpip_adapter_init();
	ESP_ERROR_CHECK(esp_event_loop_create_default());
#endif

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	//ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

	wifi_config_t wifi_config = {
		.ap = {
			.ssid = CONFIG_ESP_WIFI_SSID,
			.ssid_len = strlen(CONFIG_ESP_WIFI_SSID),
			.password = CONFIG_ESP_WIFI_PASSWORD,
			.max_connection = CONFIG_ESP_MAX_STA_CONN,
			.authmode = WIFI_AUTH_WPA_WPA2_PSK
		},
	};
	if (strlen(CONFIG_ESP_WIFI_PASSWORD) == 0) {
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	}

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s",
			 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
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

	ESP_LOGI(TAG,"ESP-IDF Ver%d.%d", ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR);

#if ESP_IDF_VERSION_MAJOR >= 4 && ESP_IDF_VERSION_MINOR >= 1
	ESP_LOGI(TAG,"ESP-IDF esp_netif");
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_t *netif = esp_netif_create_default_wifi_sta();
#else
	ESP_LOGI(TAG,"ESP-IDF tcpip_adapter");
	tcpip_adapter_init();
	ESP_ERROR_CHECK(esp_event_loop_create_default());
#endif

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

#if ESP_IDF_VERSION_MAJOR >= 4 && ESP_IDF_VERSION_MINOR >= 1
	/* Stop DHCP client */
	ESP_ERROR_CHECK(esp_netif_dhcpc_stop(netif));
	ESP_LOGI(TAG, "Stop DHCP Services");

	/* Set STATIC IP Address */
	esp_netif_ip_info_t ip_info;
	IP4_ADDR(&ip_info.ip, ip[0], ip[1], ip[2], ip[3]);
	IP4_ADDR(&ip_info.gw, gw[0], gw[1], gw[2], gw[3]);
	IP4_ADDR(&ip_info.netmask, nm[0], nm[1], nm[2], nm[3]);
	//tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
	esp_netif_set_ip_info(netif, &ip_info);

#else
	/* Stop DHCP client */
	tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);

	/* Set STATIC IP Address */
	tcpip_adapter_ip_info_t ipInfo;
	IP4_ADDR(&ipInfo.ip, ip[0], ip[1], ip[2], ip[3]);
	IP4_ADDR(&ipInfo.gw, gw[0], gw[1], gw[2], gw[3]);
	IP4_ADDR(&ipInfo.netmask, nm[0], nm[1], nm[2], nm[3]);
	tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfo);
#endif

	/*
	I referred from here.
	https://www.esp32.com/viewtopic.php?t=5380

	if we should not be using DHCP (for example we are using static IP addresses),
	then we need to instruct the ESP32 of the locations of the DNS servers manually.
	Google publicly makes available two name servers with the addresses of 8.8.8.8 and 8.8.4.4.
	*/

	ip_addr_t d;
	d.type = IPADDR_TYPE_V4;
	d.u_addr.ip4.addr = 0x08080808; //8.8.8.8 dns
	dns_setserver(0, &d);
	d.u_addr.ip4.addr = 0x08080404; //8.8.4.4 dns
	dns_setserver(1, &d);

#endif

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = CONFIG_ESP_WIFI_SSID,
			.password = CONFIG_ESP_WIFI_PASSWORD
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start() );

	ESP_LOGI(TAG, "wifi_init_sta finished.");
	ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
			 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);

	// wait for IP_EVENT_STA_GOT_IP
	while(1) {
		/* Wait forever for WIFI_CONNECTED_BIT to be set within the event group.
		   Clear the bits beforeexiting. */
		EventBits_t uxBits = xEventGroupWaitBits(s_wifi_event_group,
		   WIFI_CONNECTED_BIT, /* The bits within the event group to waitfor. */
		   pdTRUE,		  /* WIFI_CONNECTED_BIT should be cleared before returning. */
		   pdFALSE,		  /* Don't waitfor both bits, either bit will do. */
		   portMAX_DELAY);/* Wait forever. */
		if ( ( uxBits & WIFI_CONNECTED_BIT ) == WIFI_CONNECTED_BIT ){
			ESP_LOGI(TAG, "WIFI_CONNECTED_BIT");
			break;
	   }
	}
	ESP_LOGI(TAG, "Got IP Address.");
}

void initialise_mdns(void)
{
	//initialize mDNS
	ESP_ERROR_CHECK( mdns_init() );
	//set mDNS hostname (required if you want to advertise services)
	ESP_ERROR_CHECK( mdns_hostname_set(CONFIG_MDNS_HOSTNAME) );
	ESP_LOGI(TAG, "mdns hostname set to: [%s]", CONFIG_MDNS_HOSTNAME);

#if 0
	//set default mDNS instance name
	ESP_ERROR_CHECK( mdns_instance_name_set("ESP32 with mDNS") );
#endif
}
#endif

void mqtt_server(void *pvParameters);
void http_server(void *pvParameters);
void mqtt_subscriber(void *pvParameters);
void mqtt_publisher(void *pvParameters);

wl_handle_t mountFATFS(char * partition_label, char * mount_point) {
	ESP_LOGI(TAG, "Initializing FAT file system");
	// To mount device we need name of device partition, define base_path
	// and allow format partition in case if it is new one and was not formated before
	const esp_vfs_fat_mount_config_t mount_config = {
		.max_files = 4,
		.format_if_mount_failed = true,
		.allocation_unit_size = CONFIG_WL_SECTOR_SIZE
	};
	wl_handle_t s_wl_handle;
	esp_err_t err = esp_vfs_fat_spiflash_mount(mount_point, partition_label, &mount_config, &s_wl_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
		return -1;
	}
	ESP_LOGI(TAG, "Mount FAT filesystem on %s", mount_point);
	ESP_LOGI(TAG, "s_wl_handle=%d",s_wl_handle);
	return s_wl_handle;
}


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
	initialise_mdns();
	tcpip_adapter_ip_info_t ip_info;
	ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
	ESP_LOGI(TAG, "ESP32 is STA MODE");
#endif

	/* Print the local IP address */
	ESP_LOGI(TAG, "IP Address : %s", ip4addr_ntoa(&ip_info.ip));
	ESP_LOGI(TAG, "Subnet mask: %s", ip4addr_ntoa(&ip_info.netmask));
	ESP_LOGI(TAG, "Gateway	  : %s", ip4addr_ntoa(&ip_info.gw));

	// Initializing FAT file system
	char *partition_label = "storage";
	wl_handle_t s_wl_handle = mountFATFS(partition_label, MOUNT_POINT);
	if (s_wl_handle < 0) {
		ESP_LOGE(TAG, "mountFATFS fail");
		while(1) { vTaskDelay(1); }
	}

	/* Start MQTT Server using tcp transport */
	ESP_LOGI(TAG, "MQTT broker started on %s using Mongoose v%s", ip4addr_ntoa(&ip_info.ip), MG_VERSION);
	xTaskCreate(mqtt_server, "BROKER", 1024*4, NULL, 2, NULL);
	vTaskDelay(10);	// You need to wait until the task launch is complete.

	/* Start HTTP Server using mongoose */
	ESP_LOGI(TAG, "HTTP server started on %s using Mongoose v%s", ip4addr_ntoa(&ip_info.ip), MG_VERSION);
	char cparam0[64];
	sprintf(cparam0, "http://%s:8000", ip4addr_ntoa(&ip_info.ip));
	xTaskCreate(http_server, "HTTP", 1024*4, (void *)cparam0, 2, NULL);
	vTaskDelay(10);	// You need to wait until the task launch is complete.

#if CONFIG_SUBSCRIBE
	/* Start Subscriber */
	char cparam1[64];
	sprintf(cparam1, "mqtt://%s:1883", ip4addr_ntoa(&ip_info.ip));
	xTaskCreate(mqtt_subscriber, "SUBSCRIBE", 1024*4, (void *)cparam1, 2, NULL);
	vTaskDelay(10);	// You need to wait until the task launch is complete.
#endif

#if CONFIG_PUBLISH
	/* Start Publisher */
	char cparam2[64];
	sprintf(cparam2, "mqtt://%s:1883", ip4addr_ntoa(&ip_info.ip));
	xTaskCreate(mqtt_publisher, "PUBLISH", 1024*4, (void *)cparam2, 2, NULL);
	vTaskDelay(10);	// You need to wait until the task launch is complete.
#endif
}
