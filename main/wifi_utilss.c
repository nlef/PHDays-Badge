#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include <inttypes.h>
#include <mbedtls/md.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "wifi_utilss.h"

#define ESP_WIFI_CHANNEL CONFIG_ESP_WIFI_CHANNEL
#define MAX_STA_CONN CONFIG_ESP_MAX_STA_CONN

static const char* TAG = "wifi softAP";
uint8_t wifiClientConnectCounter = 0;

unsigned char ESP_WIFI_SSID[16] = "phd2_1234567890";
uint32_t ESP_WIFI_SSID_INT = 0;
uint8_t ESP_WIFI_SSID_bytes[4] = { 0 };
unsigned char ESP_WIFI_PASS[11] = "1234567890";
uint32_t ESP_WIFI_PASS_INT = 0;

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
        if (wifiClientConnectCounter < 255)
            wifiClientConnectCounter++;
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

int lenHelper(uint32_t x)
{
    if (x >= 1000000000)
        return x;
    if (x >= 100000000)
        return x * 10;
    if (x >= 10000000)
        return x * 10;
    if (x >= 1000000)
        return x * 1000;
    if (x >= 100000)
        return x * 10000;
    if (x >= 10000)
        return x * 100000;
    if (x >= 1000)
        return x * 1000000;
    if (x >= 100)
        return x * 10000000;
    if (x >= 10)
        return x * 100000000;
    return 1;
}

void getDeviceId()
{
    char* key = "someBadgeKey";

    uint8_t mac_base[6] = { 0 };
    esp_err_t ret = esp_efuse_mac_get_default(mac_base);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get base MAC address from EFUSE BLK3. (%s)", esp_err_to_name(ret));
    }

    char* payload = (char*)mac_base;
    u_char hmacResult[32];

    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

    const size_t payloadLength = strlen(payload);
    const size_t keyLength = strlen(key);

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char*)key, keyLength);
    mbedtls_md_hmac_update(&ctx, (const unsigned char*)payload, payloadLength);
    mbedtls_md_hmac_finish(&ctx, hmacResult);
    mbedtls_md_free(&ctx);

    ESP_LOGI(TAG, "mac hash");
    for (int i = 0; i < sizeof(hmacResult); i++) {
        ESP_LOGI(TAG, "%02x", (int)hmacResult[i]);
    }

    uint32_t ssid = lenHelper(hmacResult[0] | (hmacResult[1] << 8) | (hmacResult[2] << 16) | (hmacResult[3] << 24));
    uint32_t pwd = lenHelper(hmacResult[28] | (hmacResult[29] << 8) | (hmacResult[30] << 16) | (hmacResult[31] << 24));
    ESP_LOGI(TAG, "ssid from hash %lu", ssid);
    ESP_LOGI(TAG, "pwd from hash  %lu", pwd);
    ESP_WIFI_SSID_bytes[0] = hmacResult[0];
    ESP_WIFI_SSID_bytes[1] = hmacResult[1];
    ESP_WIFI_SSID_bytes[2] = hmacResult[2];
    ESP_WIFI_SSID_bytes[3] = hmacResult[3];
    sprintf((char*)ESP_WIFI_SSID, "phd2_%lu", ssid);
    sprintf((char*)ESP_WIFI_PASS, "%lu", pwd);
    ESP_WIFI_PASS_INT = pwd;
    ESP_WIFI_SSID_INT = ssid;
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .ap = {   
            .ssid_len = strlen((const char*)ESP_WIFI_SSID),
            .channel = ESP_WIFI_CHANNEL,        
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = true,
            },
        },
    };

    memcpy(wifi_config.ap.ssid, ESP_WIFI_SSID, strlen((const char*)ESP_WIFI_SSID));
    memcpy(wifi_config.ap.password, ESP_WIFI_PASS, strlen((const char*)ESP_WIFI_PASS));

    if (strlen((const char*)ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(8)); // 2dbm tx power!

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d", ESP_WIFI_SSID, ESP_WIFI_PASS, ESP_WIFI_CHANNEL);
}

void initWiFi()
{
    getDeviceId();
    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();
}