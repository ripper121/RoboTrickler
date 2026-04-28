#pragma once

#include <stddef.h>

#include "esp_err.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Connect to WiFi using credentials from the app config.
 *
 * Initialises NVS, the TCP/IP stack, the WiFi driver, and the event loop if
 * needed, then connects in station mode. If cfg->wifi.static_ip is non-empty
 * the interface is configured with the supplied static IP, gateway, subnet
 * and DNS; otherwise DHCP is used.
 *
 * Blocks until the connection succeeds or the attempt times out.
 *
 * @param cfg  Pointer to the loaded app_config_t (must not be NULL).
 * @return
 *   - ESP_OK            on successful connection + IP assignment
 *   - ESP_ERR_TIMEOUT   if the connection did not complete in time
 *   - ESP_FAIL          on any other error
 */
esp_err_t wifi_connect(const app_config_t *cfg);

/**
 * @brief Disconnect from WiFi and release resources.
 *
 * @return ESP_OK on success.
 */
esp_err_t wifi_disconnect(void);

/**
 * @brief Query the current station IPv4 address.
 *
 * @param[out] ip_str  Destination buffer for dotted IPv4 string.
 * @param[in]  ip_str_size Size of @p ip_str in bytes.
 * @return
 *   - ESP_OK            if connected and an IPv4 address is available
 *   - ESP_ERR_NOT_FOUND if WiFi is not connected
 *   - ESP_ERR_INVALID_ARG on invalid arguments
 */
esp_err_t wifi_get_ip_address(char *ip_str, size_t ip_str_size);
esp_err_t wifi_get_connected_ssid(char *ssid_str, size_t ssid_str_size);
esp_err_t wifi_get_rssi(int *rssi_dbm);

#ifdef __cplusplus
}
#endif
