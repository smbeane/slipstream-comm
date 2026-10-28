#include "bluetooth.hpp"
#include "a2dp.hpp"

#include <esp_a2dp_api.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_bt_device.h>
#include <esp_gap_bt_api.h>
#include <esp_hf_client_api.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_avrc_api.h>

namespace {

constexpr const char *TAG = "bluetooth";


// void on_hf_client_event(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param) {
    //     if (event <= ESP_HF_CLIENT_PROF_STATE_EVT) {
        //         ESP_LOGI(TAG, "APP HFP event: %s", c_hf_evt_str[event]);
        //     } else {
            //         ESP_LOGI(TAG, "APP HFP invalid event %d", event);
            //     }
            // }
            
        } // namespace
        
namespace bluetooth {
    
    AudioDataCallback bt_audio_data_callback = nullptr;

    esp_err_t initialize()
    {
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

        ESP_LOGI(TAG, "Bluetooth controller and Bluedroid initialized");
        return ESP_OK;
    }

    esp_err_t start()
    {
    
        // initialize GAP device
        ESP_ERROR_CHECK(esp_bt_gap_set_device_name("Slipstream"));
        // should initialized a esp_bt_gap_register_callback

        a2dp::start();

        // set callback function for HFP event
        // esp_hf_client_register_callback()
        
        // set scan mode to connectable and discoverable
        ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(
            ESP_BT_CONNECTABLE,
            ESP_BT_GENERAL_DISCOVERABLE));


        ESP_LOGI(TAG, "A2DP sink started");
        return ESP_OK;
    }

    esp_err_t register_audio_data_callback(AudioDataCallback callback)
    {
        if (callback == nullptr) {
            return ESP_ERR_INVALID_ARG;
        }

        bt_audio_data_callback = callback;
        return ESP_OK;
    }

    esp_err_t stop()
    {
        ESP_ERROR_CHECK(esp_a2d_sink_deinit());
        ESP_ERROR_CHECK(esp_avrc_ct_deinit());
        ESP_ERROR_CHECK(esp_bluedroid_disable());
        ESP_LOGI(TAG, "A2DP sink stopped");
        return ESP_OK;
    }

} // namespace bluetooth