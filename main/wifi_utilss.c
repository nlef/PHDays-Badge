#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "nvs_utils.h"
#include <inttypes.h>
#include <mbedtls/md.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "ota.h"
#include "wifi_utilss.h"

#define ESP_WIFI_CHANNEL CONFIG_ESP_WIFI_CHANNEL
#define MAX_STA_CONN CONFIG_ESP_MAX_STA_CONN
#define DEFAULT_SSID CONFIG_OTA_SSID
#define DEFAULT_PWD CONFIG_OTA_PASSWORD
#define DEFAULT_RSSI -127
#define DEFAULT_RSSI_5G_ADJUSTMENT 0

#define STORAGE_NAMESPACE "storage"

static const char *TAG = "wifi softAP";
uint8_t wifiClientConnectCounter = 0;

unsigned char ESP_WIFI_SSID[16] = "phd2_1234567890";
uint32_t ESP_WIFI_SSID_INT = 0;
uint8_t ESP_WIFI_SSID_bytes[4] = {0};
unsigned char ESP_WIFI_PASS[11] = "1234567890";
uint64_t ESP_WIFI_PASS_LONG = 0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
        if (wifiClientConnectCounter < 255)
            wifiClientConnectCounter++;
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "sta started");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "sta scan done");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "sta connected");
        xTaskCreate(&ota_update_custom_task, "ota_update_task", 8192, NULL, 5, NULL);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "sta got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

uint32_t lenHelper(uint32_t x) {
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

uint32_t getNewInt() {
    uint32_t randNum = esp_random();
    while (true) {
        if (randNum > 100000)
            return randNum;
        else
            randNum = esp_random();
    }
}

uint64_t getNewWifiPassword() {
    uint32_t randNum = esp_random();
    ESP_LOGI(TAG, "%lu", randNum);
    for (uint8_t i = 0; i < 250; i++) {
        randNum = esp_random();
        ESP_LOGI(TAG, "%lu", randNum);
    }
    uint32_t pwd_low = getNewInt() % 100000;
    uint32_t pwd_high = getNewInt() % 100000;
    uint64_t full_pwd = pwd_high * 100000 + pwd_low;
    ESP_LOGI(TAG, "pwd_full_new: %llu", full_pwd);
    return full_pwd;
}

uint64_t getWifiPassword() {
    nvs_handle_t my_handle;
    esp_err_t err;
    uint64_t newPasswd = getNewWifiPassword();
    uint64_t resPasswd = 0;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
        return newPasswd;

    uint64_t storedPasswd = 0;
    err = nvs_get_u64(my_handle, "ap_password", &storedPasswd);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return newPasswd;

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = nvs_set_u64(my_handle, "ap_password", newPasswd);
        if (err != ESP_OK)
            return newPasswd;
        resPasswd = newPasswd;
    } else {
        resPasswd = storedPasswd;
    }

    err = nvs_commit(my_handle);
    if (err != ESP_OK)
        return resPasswd;

    nvs_close(my_handle);
    ESP_LOGI(TAG, "pwd_full_restored: %llu", resPasswd);
    return resPasswd;
}

void getDeviceId() {
    char *key = "someBadgeKey";

    uint8_t mac_base[6] = {0};
    esp_err_t ret = esp_efuse_mac_get_default(mac_base);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get base MAC address from EFUSE BLK3. (%s)", esp_err_to_name(ret));
    }

    char *payload = (char *)mac_base;
    u_char hmacResult[32];

    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

    const size_t payloadLength = strlen(payload);
    const size_t keyLength = strlen(key);

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char *)key, keyLength);
    mbedtls_md_hmac_update(&ctx, (const unsigned char *)payload, payloadLength);
    mbedtls_md_hmac_finish(&ctx, hmacResult);
    mbedtls_md_free(&ctx);

    uint32_t ssid = lenHelper(hmacResult[0] | (hmacResult[1] << 8) | (hmacResult[2] << 16) | (hmacResult[3] << 24));

    uint64_t pwd = getWifiPassword();
    ESP_LOGI(TAG, "ssid from hash %lu", ssid);
    ESP_LOGI(TAG, "pwd from hash  %llu", pwd);
    ESP_WIFI_SSID_bytes[0] = hmacResult[0];
    ESP_WIFI_SSID_bytes[1] = hmacResult[1];
    ESP_WIFI_SSID_bytes[2] = hmacResult[2];
    ESP_WIFI_SSID_bytes[3] = hmacResult[3];
    sprintf((char *)ESP_WIFI_SSID, "phd2_%lu", ssid);
    sprintf((char *)ESP_WIFI_PASS, "%llu", pwd);
    ESP_WIFI_PASS_LONG = pwd;
    ESP_WIFI_SSID_INT = ssid;
}

void wifi_init_softap(void) {

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config_ap = {
        .ap =
            {
                .ssid_len = strlen((const char *)ESP_WIFI_SSID),
                .channel = ESP_WIFI_CHANNEL,
                .max_connection = MAX_STA_CONN,
                .authmode = WIFI_AUTH_WPA2_PSK,
                .pmf_cfg =
                    {
                        .required = true,
                    },
            },
    };
    wifi_config_t wifi_config_sta = {
        .sta =
            {
                .ssid = DEFAULT_SSID,
                .password = DEFAULT_PWD,
                .scan_method = WIFI_FAST_SCAN,
                .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
                .threshold.rssi = DEFAULT_RSSI,
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                .threshold.rssi_5g_adjustment = DEFAULT_RSSI_5G_ADJUSTMENT,
            },
    };

    memcpy(wifi_config_ap.ap.ssid, ESP_WIFI_SSID, strlen((const char *)ESP_WIFI_SSID));
    memcpy(wifi_config_ap.ap.password, ESP_WIFI_PASS, strlen((const char *)ESP_WIFI_PASS));

    if (strlen((const char *)ESP_WIFI_PASS) == 0) {
        wifi_config_ap.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config_ap));

    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));

    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT40));

    ESP_ERROR_CHECK(esp_wifi_config_80211_tx_rate(WIFI_IF_AP, WIFI_PHY_RATE_54M));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT40));
    ESP_ERROR_CHECK(esp_wifi_config_80211_tx_rate(WIFI_IF_STA, WIFI_PHY_RATE_54M));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(8)); // 2dbm tx power!

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d", ESP_WIFI_SSID, ESP_WIFI_PASS, ESP_WIFI_CHANNEL);
}

void initWiFi() {
    getDeviceId();
    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();
}
