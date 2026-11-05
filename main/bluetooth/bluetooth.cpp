#include "bluetooth.hpp"
#include "a2dp.hpp"
#include "hfp.hpp"

#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_bt_device.h>
#include <esp_gap_bt_api.h>
#include <esp_log.h>
#include <nvs_flash.h>

bool use_a2dp = true;
uint32_t input_freq = 48000;

extern void play_chime(const char* sound_name);

namespace {
    
    constexpr const char *TAG = "bluetooth";
    
} // namespace

namespace bluetooth {
    
    AudioDataCallback bt_audio_data_callback = nullptr;

    esp_err_t initialize() {
        // bluetooth needs nvs memory??
        ESP_ERROR_CHECK(nvs_flash_init());
        
        esp_bt_controller_config_t bt_config =
        BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_bt_controller_init(&bt_config));
        ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));
        
        esp_bluedroid_config_t bluedroid_config =
        BT_BLUEDROID_INIT_CONFIG_DEFAULT();
        bluedroid_config.ssp_en = true;
        
        ESP_ERROR_CHECK(esp_bluedroid_init_with_cfg(&bluedroid_config));
        ESP_ERROR_CHECK(esp_bluedroid_enable());
        
        a2dp::init();
        hfp::init();

        ESP_LOGI(TAG, "Bluetooth controller and Bluedroid initialized");
        return ESP_OK;
    }

    void gap_event_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
        switch (event) {
            case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT:
                // Check if the connection was successful (status == 0)
                if (param->acl_conn_cmpl_stat.stat == ESP_BT_STATUS_SUCCESS) {
                    ESP_LOGI(TAG, "Physical ACL Link Connected!");
                    play_chime("connect");
                } else {
                    ESP_LOGW(TAG, "ACL Connection Failed, status: %d", param->acl_conn_cmpl_stat.stat);
                }
                break;

            case ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT:
                // Check if the disconnection was successful
                ESP_LOGI(TAG, "Disconnecting");
                play_chime("disconnect");
                break;
            default: {
                break;
            }
        }
    }

    esp_err_t start() {
    
        // initialize GAP device
        ESP_ERROR_CHECK(esp_bt_gap_set_device_name("Slipstream"));
        ESP_ERROR_CHECK(esp_bt_gap_register_callback(gap_event_callback));

        a2dp::start();
        hfp::start();

        // set callback function for HFP event
        // esp_hf_client_register_callback()
        
        // set scan mode to connectable and discoverable
        ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(
            ESP_BT_CONNECTABLE,
            ESP_BT_GENERAL_DISCOVERABLE));

        // ESP_LOGI(TAG, "A2DP sink started");
        return ESP_OK;
    }

    esp_err_t register_audio_data_callback(AudioDataCallback callback) {
        if (callback == nullptr) {
            return ESP_ERR_INVALID_ARG;
        }

        bt_audio_data_callback = callback;
        return ESP_OK;
    }

    esp_err_t stop() {
        a2dp::stop();
        hfp::stop();

        ESP_ERROR_CHECK(esp_bluedroid_disable());
        ESP_LOGI(TAG, "A2DP sink stopped");
        return ESP_OK;
    }

} // namespace bluetooth