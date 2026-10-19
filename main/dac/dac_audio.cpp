#include "dac_audio.hpp"

#include <esp_log.h>
#include "driver/dac_continuous.h"

namespace {

constexpr const char *TAG = "dac_audio";

dac_continuous_handle_t s_dac_handle = nullptr;

} // namespace

esp_err_t configure_dac(void)
{
    if (s_dac_handle != nullptr) {
        return ESP_OK;
    }

    // configure dac to output with basic config
    dac_continuous_config_t config = {
        .chan_mask = DAC_CHANNEL_MASK_CH0,
        .desc_num = 8,
        .buf_size = 512,
        .freq_hz = 44100,
        .offset = 0,
        .clk_src = DAC_DIGI_CLK_SRC_DEFAULT,
        .chan_mode = DAC_CHANNEL_MODE_SIMUL,
    };

    esp_err_t err =
        dac_continuous_new_channels(&config, &s_dac_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "dac_continuous_new_channels failed: %s",
                 esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "DAC configured");
    return ESP_OK;
}

esp_err_t enable_dac_audio(void)
{
    if (s_dac_handle == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = dac_continuous_enable(s_dac_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "dac_continuous_enable failed: %s",
                 esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "DAC audio enabled");
    return ESP_OK;
}

esp_err_t disable_dac_audio(void)
{
    if (s_dac_handle == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = dac_continuous_disable(s_dac_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "dac_continuous_disable failed: %s",
                 esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "DAC audio disabled");
    return ESP_OK;
}

esp_err_t write_audio_to_dac(uint8_t *data, size_t size)
{
    if (s_dac_handle == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    if (data == nullptr && size != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (size == 0) {
        return ESP_OK;
    }

    size_t bytes_loaded = 0;

    esp_err_t err = dac_continuous_write(
        s_dac_handle,
        data,
        size,
        &bytes_loaded,
        100); 
        
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "dac_continuous_write failed: %s",
                 esp_err_to_name(err));
        return err;
    }

    if (bytes_loaded != size) {
        ESP_LOGW(TAG,
                 "DAC loaded %zu/%zu bytes",
                 bytes_loaded,
                 size);
    }

    return ESP_OK;
}