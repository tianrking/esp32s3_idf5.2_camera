#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "index_html.h"  // 引入HTML文件

#define TAG "camera_http"

// Wi-Fi 配置
#define WIFI_SSID "KUNIU"
#define WIFI_PASS "kuniu666"

// GPIO 引脚定义
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    15
#define SIOD_GPIO_NUM    4
#define SIOC_GPIO_NUM    5

#define Y9_GPIO_NUM      16
#define Y8_GPIO_NUM      17
#define Y7_GPIO_NUM      18
#define Y6_GPIO_NUM      12
#define Y5_GPIO_NUM      10
#define Y4_GPIO_NUM      8
#define Y3_GPIO_NUM      9
#define Y2_GPIO_NUM      11
#define VSYNC_GPIO_NUM   6
#define HREF_GPIO_NUM    7
#define PCLK_GPIO_NUM    13

static camera_config_t camera_config = {
    .pin_pwdn     = PWDN_GPIO_NUM,
    .pin_reset    = RESET_GPIO_NUM,
    .pin_xclk     = XCLK_GPIO_NUM,
    .pin_sccb_sda = SIOD_GPIO_NUM,
    .pin_sccb_scl = SIOC_GPIO_NUM,

    .pin_d7     = Y9_GPIO_NUM,
    .pin_d6     = Y8_GPIO_NUM,
    .pin_d5     = Y7_GPIO_NUM,
    .pin_d4     = Y6_GPIO_NUM,
    .pin_d3     = Y5_GPIO_NUM,
    .pin_d2     = Y4_GPIO_NUM,
    .pin_d1     = Y3_GPIO_NUM,
    .pin_d0     = Y2_GPIO_NUM,
    .pin_vsync  = VSYNC_GPIO_NUM,
    .pin_href   = HREF_GPIO_NUM,
    .pin_pclk   = PCLK_GPIO_NUM,

    .xclk_freq_hz = 20000000,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_QQVGA,
    .jpeg_quality = 12,
    .fb_count = 1,
    .fb_location = CAMERA_FB_IN_DRAM,
};

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Reconnecting to WiFi...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    }
}

static void wifi_init() {
    wifi_event_group = xEventGroupCreate();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

// 替换原先的 stream_handler 为 capture_handler
static esp_err_t capture_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;

    fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    res = httpd_resp_set_type(req, "image/jpeg");
    if (res == ESP_OK) {
        res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    }
    esp_camera_fb_return(fb);
    return res;
}

static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char *)main_index_html, main_index_html_len);
}

static esp_err_t control_handler(httpd_req_t *req) {
    char buf[32];
    int ret = httpd_req_get_url_query_str(req, buf, sizeof(buf));
    if (ret == ESP_OK) {
        char cmd[16];
        if (httpd_query_key_value(buf, "cmd", cmd, sizeof(cmd)) == ESP_OK) {
            ESP_LOGI(TAG, "Command received: %s", cmd);
            // 在这里添加控制逻辑，例如驱动电机或执行特定操作
            if (strcmp(cmd, "forward") == 0) {
                // 向前移动的代码
            } else if (strcmp(cmd, "backward") == 0) {
                // 向后移动的代码
            } else if (strcmp(cmd, "left") == 0) {
                // 左转的代码
            } else if (strcmp(cmd, "right") == 0) {
                // 右转的代码
            } else if (strcmp(cmd, "stop") == 0) {
                // 停止的代码
            }
        }
    }
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

static esp_err_t start_camera_server() {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    esp_err_t ret = httpd_start(&server, &config);
    if (ret == ESP_OK) {
        httpd_uri_t uri_index = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_index);

        httpd_uri_t uri_capture = {
            .uri = "/capture",
            .method = HTTP_GET,
            .handler = capture_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_capture);

        httpd_uri_t uri_control = {
            .uri = "/control",
            .method = HTTP_GET,
            .handler = control_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_control);

        ESP_LOGI(TAG, "Camera server started");
    }
    return ret;
}

void app_main() {
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS");
        return;
    }

    // 初始化Wi-Fi
    wifi_init();
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    // 初始化摄像头
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return;
    }

    // 启动Web服务器
    start_camera_server();
}
