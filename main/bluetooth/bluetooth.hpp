#pragma once

#include <cstddef>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <esp_err.h>

namespace bluetooth {

extern bool is_currently_playing;

using AudioDataCallback = void (*)(const uint8_t *pcm, size_t pcm_size);

static QueueHandle_t avrcp_queue = NULL;
static uint8_t tl = 0;

esp_err_t initialize(void);
esp_err_t start(void);
esp_err_t send_avrc_passthrough_cmd(uint8_t cmd, bool is_pressed);
esp_err_t register_audio_data_callback(AudioDataCallback callback);
esp_err_t stop(void);

} // namespace bluetooth
