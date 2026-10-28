#pragma once

#include <cstddef>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <esp_err.h>


namespace a2dp{
    
    extern bool is_currently_playing;

    esp_err_t send_avrc_passthrough_cmd(uint8_t cmd, bool is_pressed);
    void init(void);
    void start(void);

    static QueueHandle_t avrcp_queue = NULL;
    static uint8_t tl = 0;
}
