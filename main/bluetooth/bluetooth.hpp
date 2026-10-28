#pragma once

#include <cstddef>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <esp_err.h>

namespace bluetooth {

using AudioDataCallback = void (*)(const uint8_t *pcm, size_t pcm_size);
extern AudioDataCallback bt_audio_data_callback;

esp_err_t initialize();
esp_err_t start();
esp_err_t register_audio_data_callback(AudioDataCallback callback);
esp_err_t stop();

} // namespace bluetooth
