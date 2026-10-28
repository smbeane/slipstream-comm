#pragma once

#include <cstddef>
#include <cstdint>

#include <esp_err.h>

esp_err_t configure_dac();
esp_err_t enable_dac_audio();
esp_err_t disable_dac_audio();
esp_err_t write_audio_to_dac(uint8_t *data, size_t size);
