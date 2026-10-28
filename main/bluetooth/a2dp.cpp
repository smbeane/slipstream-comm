#include "a2dp.hpp"
#include "bluetooth.hpp"

#include <esp_a2dp_api.h>
#include <esp_avrc_api.h>
#include <esp_log.h>

extern void play_chime(const char* sound_name);

namespace {

    constexpr const char *TAG = "bluetooth_a2dp";

}

namespace a2dp {

    bool is_currently_playing = false;

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
                is_currently_playing = true;
            } else if (param->audio_stat.state == 0) {
                is_currently_playing = false;
            } else {
                is_currently_playing = false;
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

    esp_err_t send_avrc_passthrough_cmd(uint8_t cmd, bool is_pressed) {
        return esp_avrc_ct_send_passthrough_cmd(tl++, cmd, is_pressed);
    }

    void on_a2dp_audio_data(const uint8_t *pcm, uint32_t pcm_size)
    {
        if (bluetooth::bt_audio_data_callback != nullptr) {
            bluetooth::bt_audio_data_callback(pcm, static_cast<size_t>(pcm_size));
        }
    }

    void init() {
        avrcp_queue = xQueueCreate(5, sizeof(uint8_t));
    }

    void start() {
        // set callback for A2DP events
        ESP_ERROR_CHECK(esp_a2d_register_callback(on_a2dp_event));
        ESP_ERROR_CHECK(esp_a2d_sink_register_data_callback(on_a2dp_audio_data));

        // set callback function for AVRCP events
        esp_avrc_ct_register_callback(on_avrcp_ct);
        assert(esp_avrc_ct_init() == ESP_OK);

        // init sink
        ESP_ERROR_CHECK(esp_a2d_sink_init());
    }

} // namespace a2dp