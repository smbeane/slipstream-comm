#pragma once

#include <cstddef>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <esp_err.h>
#include <esp_avrc_api.h>
#include <driver/gpio.h>


namespace controls {
    void init_hardware();
    void button_handler(void* arg);
}