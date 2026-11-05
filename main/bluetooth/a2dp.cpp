#include "a2dp.hpp"
#include "bluetooth.hpp"

#include <esp_a2dp_api.h>
#include <esp_avrc_api.h>
#include <esp_log.h>

extern uint32_t input_freq;
extern bool use_a2dp;

extern void play_chime(const char* sound_name);

namespace {

    constexpr const char *TAG = "bluetooth_a2dp";

}

namespace a2dp {

    bool is_currently_playing = false;
    uint32_t a2dp_freq;

    void a2dp_event_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
        switch (event) {
        case ESP_A2D_AUDIO_STATE_EVT: {
            ESP_LOGI(TAG, "A2DP audio state: %d", param->audio_stat.state);
            if (param->audio_stat.state == 1) {
                is_currently_playing = true;
                enable_a2dp();
            } else if (param->audio_stat.state == 0) {
                is_currently_playing = false;
            } else {
                is_currently_playing = false;
            }
            break;
        } case ESP_A2D_AUDIO_CFG_EVT: {
            ESP_LOGI(TAG, "A2DP audio stream configured");
            esp_a2d_cb_param_t *a2d = (esp_a2d_cb_param_t *)(param);

            if (a2d->audio_cfg.mcc.type == ESP_A2D_MCT_SBC) {
                uint8_t sbc_samp_freq = a2d->audio_cfg.mcc.cie.sbc_info.samp_freq;

                switch (sbc_samp_freq) {
                    case(ESP_A2D_SBC_CIE_SF_16K):
                        input_freq = 16000;
                        a2dp_freq = 16000;
                        break;
                    case (ESP_A2D_SBC_CIE_SF_32K):
                        input_freq = 32000;
                        a2dp_freq = 32000;
                        break;
                    case (ESP_A2D_SBC_CIE_SF_44K):
                        input_freq = 44100;
                        a2dp_freq = 44100;
                        break;
                    case (ESP_A2D_SBC_CIE_SF_48K):
                        input_freq = 48000;
                        a2dp_freq = 48000;
                        break;
                    default:
                        a2dp_freq = 44100;
                        break;
                } 

                ESP_LOGI(TAG, "A2DP stream freq: %d", sbc_samp_freq);
            }
            break;
        }
        default:
            break;
        }
    }

    void enable_a2dp() {
        a2dp::send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_PLAY, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
        a2dp::send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_PLAY, 1);

        input_freq = a2dp_freq;
        use_a2dp = true;
    }

    void on_avrcp_ct(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param) {
        ESP_LOGI(TAG, "AVRC Event: %d", event);    
    }

    esp_err_t send_avrc_passthrough_cmd(uint8_t cmd, bool is_pressed) {
        return esp_avrc_ct_send_passthrough_cmd(tl++, cmd, is_pressed);
    }

    void a2dp_audio_callback(const uint8_t *pcm, uint32_t pcm_size) {
        if (bluetooth::bt_audio_data_callback != nullptr && use_a2dp) {
            bluetooth::bt_audio_data_callback(pcm, static_cast<size_t>(pcm_size));
        }
    }

    void init() {
        avrcp_queue = xQueueCreate(5, sizeof(uint8_t));
    }

    void start() {
        // set callback for A2DP events
        ESP_ERROR_CHECK(esp_a2d_register_callback(a2dp_event_callback));
        ESP_ERROR_CHECK(esp_a2d_sink_register_data_callback(a2dp_audio_callback));

        // set callback function for AVRCP events
        esp_avrc_ct_register_callback(on_avrcp_ct);
        assert(esp_avrc_ct_init() == ESP_OK);

        // init sink
        ESP_ERROR_CHECK(esp_a2d_sink_init());
    }

    void stop() {
        ESP_ERROR_CHECK(esp_a2d_sink_deinit());
        ESP_ERROR_CHECK(esp_avrc_ct_deinit());
    }

} // namespace a2dp