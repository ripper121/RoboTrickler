#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "lwip/dns.h"
#include "nvs_flash.h"

#include "wifi.h"

static const char *TAG = "wifi";

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define WIFI_MAX_RETRY      5
#define WIFI_CONNECT_TIMEOUT_MS  10000
#define WIFI_AP_CHANNEL     1
#define WIFI_AP_MAX_CONNECTIONS 4

static EventGroupHandle_t s_wifi_event_group = NULL;
static esp_netif_t       *s_sta_netif        = NULL;
static esp_netif_t       *s_ap_netif         = NULL;
static int                s_retry_count      = 0;
static bool               s_initial_connect_pending = false;
static bool               s_connected = false;
static bool               s_event_handlers_registered = false;
static esp_event_handler_instance_t s_wifi_event_handler;
static esp_event_handler_instance_t s_ip_event_handler;
static wifi_mode_t        s_wifi_mode = WIFI_MODE_NULL;

static void event_handler(void *arg, esp_event_base_t base,
                           int32_t id, void *data)
{
    (void)arg;

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        s_connected = false;
        esp_wifi_connect();

    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *event = (const wifi_event_sta_disconnected_t *)data;
        const int reason = event != NULL ? event->reason : 0;

        s_connected = false;
        if (s_wifi_event_group != NULL) {
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }

        if (s_initial_connect_pending) {
            if (s_retry_count < WIFI_MAX_RETRY) {
                s_retry_count++;
                ESP_LOGW(TAG, "Initial connect retry %d/%d (reason=%d)", s_retry_count, WIFI_MAX_RETRY, reason);
                esp_wifi_connect();
            } else if (s_wifi_event_group != NULL) {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
        } else {
            s_retry_count++;
            ESP_LOGW(TAG, "WiFi disconnected, reconnecting (attempt %d, reason=%d)", s_retry_count, reason);
            esp_wifi_connect();
        }

    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
        const wifi_event_ap_staconnected_t *event = (const wifi_event_ap_staconnected_t *)data;
        if (event != NULL) {
            ESP_LOGI(TAG, "SoftAP station " MACSTR " joined, AID=%d",
                     MAC2STR(event->mac), event->aid);
        }

    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STADISCONNECTED) {
        const wifi_event_ap_stadisconnected_t *event = (const wifi_event_ap_stadisconnected_t *)data;
        if (event != NULL) {
            ESP_LOGI(TAG, "SoftAP station " MACSTR " left, AID=%d, reason=%d",
                     MAC2STR(event->mac), event->aid, event->reason);
        }

    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        s_connected = true;
        if (s_wifi_event_group != NULL) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

static void wifi_unregister_event_handlers(void)
{
    if (!s_event_handlers_registered) {
        return;
    }

    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_ip_event_handler);
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi_event_handler);
    s_event_handlers_registered = false;
}

static esp_err_t apply_static_ip(const config_wifi_t *wcfg)
{
    if (!s_sta_netif) {
        return ESP_FAIL;
    }

    esp_netif_dhcpc_stop(s_sta_netif);

    esp_netif_ip_info_t ip_info = {0};
    ip_info.ip.addr      = ipaddr_addr(wcfg->static_ip);
    ip_info.gw.addr      = ipaddr_addr(wcfg->gateway);
    ip_info.netmask.addr = ipaddr_addr(wcfg->subnet);

    esp_err_t ret = esp_netif_set_ip_info(s_sta_netif, &ip_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_set_ip_info failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if (wcfg->dns[0] != '\0') {
        esp_netif_dns_info_t dns = {0};
        dns.ip.type = ESP_IPADDR_TYPE_V4;
        dns.ip.u_addr.ip4.addr = ipaddr_addr(wcfg->dns);
        esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_MAIN, &dns);
    }

    ESP_LOGI(TAG, "Static IP: %s  GW: %s  Mask: %s",
             wcfg->static_ip, wcfg->gateway, wcfg->subnet);
    return ESP_OK;
}

static esp_err_t wifi_init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to erase NVS: %s", esp_err_to_name(ret));
            return ret;
        }
        ret = nvs_flash_init();
    }

    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
    }

    return ret == ESP_ERR_INVALID_STATE ? ESP_OK : ret;
}

static esp_err_t wifi_init_driver(void)
{
    esp_err_t ret = wifi_init_nvs();
    if (ret != ESP_OK) {
        return ret;
    }

    /* TCP/IP stack + event loop (idempotent if already called) */
    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_event_loop_create_default: %s", esp_err_to_name(ret));
        return ret;
    }

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&init_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              &event_handler, NULL, &s_wifi_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "register wifi handler failed: %s", esp_err_to_name(ret));
        goto fail;
    }

    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              &event_handler, NULL, &s_ip_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "register ip handler failed: %s", esp_err_to_name(ret));
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi_event_handler);
        goto fail;
    }
    s_event_handlers_registered = true;

    return ESP_OK;

fail:
    (void)esp_wifi_deinit();
    return ret;
}

static esp_err_t wifi_start_sta(const config_wifi_t *wcfg)
{
    esp_err_t ret;

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (s_sta_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create default WiFi STA netif");
        return ESP_FAIL;
    }

    if (s_wifi_event_group == NULL) {
        s_wifi_event_group = xEventGroupCreate();
        if (s_wifi_event_group == NULL) {
            esp_netif_destroy_default_wifi(s_sta_netif);
            s_sta_netif = NULL;
            return ESP_ERR_NO_MEM;
        }
    }

    s_retry_count = 0;
    s_connected = false;
    s_wifi_mode = WIFI_MODE_STA;

    if (wcfg->static_ip[0] != '\0') {
        ret = apply_static_ip(wcfg);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    wifi_config_t wifi_cfg = {0};
    strncpy((char *)wifi_cfg.sta.ssid,     wcfg->ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, wcfg->psk,  sizeof(wifi_cfg.sta.password) - 1);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_GOTO_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), cleanup, TAG, "esp_wifi_set_mode failed");
    ESP_GOTO_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg), cleanup, TAG, "esp_wifi_set_config failed");

    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    s_initial_connect_pending = true;
    ESP_GOTO_ON_ERROR(esp_wifi_start(), cleanup, TAG, "esp_wifi_start failed");

    ESP_LOGI(TAG, "Connecting to SSID: %s", wcfg->ssid);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));
    s_initial_connect_pending = false;

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to %s", wcfg->ssid);
        ret = ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to %s", wcfg->ssid);
        ret = ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "Connection timed out");
        ret = ESP_ERR_TIMEOUT;
    }

cleanup:
    return ret;
}

static esp_err_t wifi_start_ap(const config_wifi_t *wcfg)
{
    esp_err_t ret;
    wifi_config_t wifi_cfg = { 0 };
    const size_t psk_len = strlen(wcfg->psk);

    s_ap_netif = esp_netif_create_default_wifi_ap();
    if (s_ap_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create default WiFi AP netif");
        return ESP_FAIL;
    }

    s_connected = false;
    s_wifi_mode = WIFI_MODE_AP;

    strncpy((char *)wifi_cfg.ap.ssid, wcfg->ssid, sizeof(wifi_cfg.ap.ssid) - 1U);
    wifi_cfg.ap.ssid_len = strlen(wcfg->ssid);
    strncpy((char *)wifi_cfg.ap.password, wcfg->psk, sizeof(wifi_cfg.ap.password) - 1U);
    wifi_cfg.ap.channel = WIFI_AP_CHANNEL;
    wifi_cfg.ap.max_connection = WIFI_AP_MAX_CONNECTIONS;
    wifi_cfg.ap.authmode = psk_len == 0U ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    wifi_cfg.ap.pmf_cfg.required = false;

    ESP_GOTO_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), cleanup, TAG, "esp_wifi_set_mode failed");
    ESP_GOTO_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg), cleanup, TAG, "esp_wifi_set_config failed");
    ESP_GOTO_ON_ERROR(esp_wifi_start(), cleanup, TAG, "esp_wifi_start failed");

    s_connected = true;
    ESP_LOGI(TAG, "SoftAP started. SSID:%s channel:%d", wcfg->ssid, WIFI_AP_CHANNEL);
    return ESP_OK;

cleanup:
    return ret;
}

esp_err_t wifi_connect(const app_config_t *cfg)
{
    esp_err_t ret;

    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_sta_netif != NULL || s_ap_netif != NULL || s_wifi_mode != WIFI_MODE_NULL) {
        (void)wifi_disconnect();
    }

    ret = wifi_init_driver();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = cfg->wifi.ap ? wifi_start_ap(&cfg->wifi) : wifi_start_sta(&cfg->wifi);
    if (ret != ESP_OK) {
        s_initial_connect_pending = false;
        (void)wifi_disconnect();
        return ret;
    }

    return ESP_OK;
}

esp_err_t wifi_disconnect(void)
{
    s_initial_connect_pending = false;
    s_connected = false;
    s_wifi_mode = WIFI_MODE_NULL;
    wifi_unregister_event_handlers();

    esp_err_t ret = esp_wifi_disconnect();

    if (ret == ESP_ERR_WIFI_NOT_INIT) {
        ret = ESP_OK;
    }

    esp_err_t stop_ret = esp_wifi_stop();
    if (stop_ret != ESP_OK &&
        stop_ret != ESP_ERR_WIFI_NOT_INIT &&
        stop_ret != ESP_ERR_WIFI_NOT_STARTED) {
        ret = stop_ret;
    }

    esp_err_t deinit_ret = esp_wifi_deinit();
    if (deinit_ret != ESP_OK &&
        deinit_ret != ESP_ERR_WIFI_NOT_INIT) {
        ret = deinit_ret;
    }

    if (s_sta_netif) {
        esp_netif_destroy_default_wifi(s_sta_netif);
        s_sta_netif = NULL;
    }
    if (s_ap_netif) {
        esp_netif_destroy_default_wifi(s_ap_netif);
        s_ap_netif = NULL;
    }
    if (s_wifi_event_group != NULL) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }
    ESP_LOGI(TAG, "Disconnected");
    return ret;
}

esp_err_t wifi_get_ip_address(char *ip_str, size_t ip_str_size)
{
    esp_netif_ip_info_t ip_info = { 0 };

    if (ip_str == NULL || ip_str_size == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    ip_str[0] = '\0';

    if (!s_connected) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_netif_t *active_netif = (s_wifi_mode == WIFI_MODE_AP) ? s_ap_netif : s_sta_netif;
    if (active_netif == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    ESP_RETURN_ON_ERROR(esp_netif_get_ip_info(active_netif, &ip_info), TAG, "esp_netif_get_ip_info failed");
    if (ip_info.ip.addr == 0U) {
        return ESP_ERR_NOT_FOUND;
    }

    snprintf(ip_str, ip_str_size, IPSTR, IP2STR(&ip_info.ip));
    return ESP_OK;
}

esp_err_t wifi_get_connected_ssid(char *ssid_str, size_t ssid_str_size)
{
    wifi_config_t wifi_cfg = { 0 };
    wifi_interface_t wifi_if;

    if (ssid_str == NULL || ssid_str_size == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    ssid_str[0] = '\0';

    if (!s_connected) {
        return ESP_ERR_NOT_FOUND;
    }

    if (s_wifi_mode == WIFI_MODE_AP) {
        wifi_if = WIFI_IF_AP;
    } else if (s_wifi_mode == WIFI_MODE_STA) {
        wifi_if = WIFI_IF_STA;
    } else {
        return ESP_ERR_NOT_FOUND;
    }

    if (esp_wifi_get_config(wifi_if, &wifi_cfg) != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }

    snprintf(ssid_str,
             ssid_str_size,
             "%s",
             (const char *)(s_wifi_mode == WIFI_MODE_AP ? wifi_cfg.ap.ssid : wifi_cfg.sta.ssid));
    return ESP_OK;
}

esp_err_t wifi_get_rssi(int *rssi_dbm)
{
    if (rssi_dbm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *rssi_dbm = 0;

    if (s_wifi_mode != WIFI_MODE_STA || s_sta_netif == NULL || !s_connected) {
        return ESP_ERR_NOT_FOUND;
    }

    return esp_wifi_sta_get_rssi(rssi_dbm);
}
