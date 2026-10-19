#pragma once

#include <cstddef>
#include <cstdint>

#include <esp_err.h>

esp_err_t configure_dac(void);
esp_err_t enable_dac_audio(void);
esp_err_t disable_dac_audio(void);
esp_err_t write_audio_to_dac(uint8_t *data, size_t size);
