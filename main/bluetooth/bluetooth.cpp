#include "bluetooth.hpp"

#include <esp_a2dp_api.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_bt_api.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_avrc_api.h>

extern void play_chime(const char* sound_name);

namespace {

constexpr const char *TAG = "bluetooth_a2dp";

bluetooth::AudioDataCallback g_audio_data_callback = nullptr;


void on_a2dp_event(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT:
        switch (param->conn_stat.state) {
            case ESP_A2D_CONNECTION_STATE_CONNECTED:
                ESP_LOGI(TAG, "A2DP Connected!");
                play_chime("connect");
                break;
            case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
                ESP_LOGI(TAG, "A2DP Disconnected...");
                play_chime("disconnect");
                break;
            default:
                break;
        }
        break;
    case ESP_A2D_AUDIO_STATE_EVT:
        ESP_LOGI(TAG, "A2DP audio state: %d", param->audio_stat.state);
        if (param->audio_stat.state == 1) {
            bluetooth::is_currently_playing = true;
        } else if (param->audio_stat.state == 0) {
            bluetooth::is_currently_playing = false;
        } else {
            bluetooth::is_currently_playing = false;
        }
        break;
    case ESP_A2D_AUDIO_CFG_EVT:
        ESP_LOGI(TAG, "A2DP audio stream configured");
        break;
    default:
        break;
    }
}

void on_avrcp_ct(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param) {
    ESP_LOGI(TAG, "AVRC Event: %d", event);    
}

void on_a2dp_audio_data(const uint8_t *pcm, uint32_t pcm_size)
{
    if (g_audio_data_callback != nullptr) {
        g_audio_data_callback(pcm, static_cast<size_t>(pcm_size));
    }
}

} // namespace

namespace bluetooth {

bool is_currently_playing = false;

esp_err_t initialize(void)
{
    // initalize memory for bluetooth
    ESP_ERROR_CHECK(nvs_flash_init());
    avrcp_queue = xQueueCreate(5, sizeof(uint8_t));

    // initialize bt with default config
    esp_bt_controller_config_t controller_config =
        BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&controller_config));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));

    // initalize bluedroid with default config
    esp_bluedroid_config_t bluedroid_config =
        BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    bluedroid_config.ssp_en = true;

    ESP_ERROR_CHECK(esp_bluedroid_init_with_cfg(&bluedroid_config));
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_LOGI(TAG, "Bluetooth controller and Bluedroid initialized");
    return ESP_OK;
}

esp_err_t start(void)
{
 
    // initialize A2DP sink with device name slipstream
    ESP_ERROR_CHECK(esp_bt_gap_set_device_name("Slipstream"));

    // set callback function for A2DP events
    ESP_ERROR_CHECK(esp_a2d_register_callback(on_a2dp_event));
    ESP_ERROR_CHECK(esp_a2d_sink_register_data_callback(on_a2dp_audio_data));

    // set callback function for AVRCP events
    esp_avrc_ct_register_callback(on_avrcp_ct);
    assert(esp_avrc_ct_init() == ESP_OK);

    // init sink
    ESP_ERROR_CHECK(esp_a2d_sink_init());

    // set scan mode to connectable and discoverable
    ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(
        ESP_BT_CONNECTABLE,
        ESP_BT_GENERAL_DISCOVERABLE));


    ESP_LOGI(TAG, "A2DP sink started");
    return ESP_OK;
}

esp_err_t send_avrc_passthrough_cmd(uint8_t cmd, bool is_pressed) {
    return esp_avrc_ct_send_passthrough_cmd(tl++, cmd, is_pressed);
}

esp_err_t register_audio_data_callback(AudioDataCallback callback)
{
    if (callback == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    g_audio_data_callback = callback;
    return ESP_OK;
}

esp_err_t stop(void)
{
    ESP_ERROR_CHECK(esp_a2d_sink_deinit());
    ESP_ERROR_CHECK(esp_avrc_ct_deinit());
    ESP_ERROR_CHECK(esp_bluedroid_disable());
    ESP_LOGI(TAG, "A2DP sink stopped");
    return ESP_OK;
}

} // namespace bluetooth