#pragma once

#include <cstddef>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <esp_err.h>

namespace hfp {

    void init ();
    void start ();

}