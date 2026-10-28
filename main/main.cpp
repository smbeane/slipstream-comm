#include <cstdint>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"

#include "bluetooth/bluetooth.hpp"
#include "dac/dac_audio.hpp"
#include "controls/controls.hpp"
#include "sounds/connect.h"
#include "sounds/disconnect.h"


#include <esp_avrc_api.h>
#include "esp_log.h"

namespace {

constexpr const char *TAG = "audio_pipeline";
constexpr size_t kConvertedBufferSize = 4096;

uint8_t converted_audio[kConvertedBufferSize];
RingbufHandle_t s_ringbuf = nullptr;

void convert_pcm_to_u8(const uint8_t *pcm, size_t pcm_size)
{
    const size_t frame_count = pcm_size / 4; 
    const int16_t *samples = reinterpret_cast<const int16_t *>(pcm);
    static int32_t quant_error = 0;

    for (size_t frame = 0; frame < frame_count; ++frame) {
        int32_t left  = samples[frame * 2];
        int32_t right = samples[frame * 2 + 1];

        // Average L+R to mono (-32768 to +32767)
        int32_t mono = (left + right) >> 1;

        // Shift to unsigned range [0, 65535] and incorporate previous error
        int32_t u16_with_err = mono + 32768 + quant_error;

        // Clamp to valid 16-bit range
        if (u16_with_err > 65535) u16_with_err = 65535;
        if (u16_with_err < 0)     u16_with_err = 0;

        // Truncate to 8-bit DAC output
        uint8_t out_sample = static_cast<uint8_t>(u16_with_err >> 8);
        converted_audio[frame] = out_sample;

        // Reconstruct the 16-bit value represented by the DAC output
        // and compute true signed quantization error: (input - reconstructed)
        int32_t reconstructed = static_cast<int32_t>(out_sample) << 8;
        quant_error = u16_with_err - reconstructed;
    }
}

void on_a2dp_audio_data(const uint8_t *pcm, size_t pcm_size)
{
    if (pcm == nullptr || pcm_size == 0 || s_ringbuf == nullptr) {
        return;
    }

    // Copy the packet into the ring buffer and return to the Bluetooth task.
    // A timeout prevents an A2DP callback from blocking indefinitely.
    if (xRingbufferSend(s_ringbuf, pcm, pcm_size, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Audio ring buffer full; dropped %zu-byte A2DP packet", pcm_size);
    }
}

void audio_playback_task(void *pvParameters)
{
    size_t item_size;

    while (true) {
        uint8_t *pcm = static_cast<uint8_t *>(
            xRingbufferReceive(s_ringbuf, &item_size, portMAX_DELAY)
        );

        if (pcm == nullptr) {
            continue;
        }

        const size_t frame_count = item_size / 4;
        if (item_size % 4 != 0 || frame_count > kConvertedBufferSize) {
            ESP_LOGW(TAG, "Skipping unsupported PCM buffer size: %zu bytes", item_size);
        } else {
            convert_pcm_to_u8(pcm, item_size);
            esp_err_t err = write_audio_to_dac(converted_audio, frame_count);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "DAC write failed: %s", esp_err_to_name(err));
            }
        }

        vRingbufferReturnItem(s_ringbuf, pcm);
    }
}

// Add this inside the anonymous namespace in main.cpp
void test_dac_output()
{
    ESP_LOGI(TAG, "Starting standalone 441Hz DAC tone test...");

    // 100 samples per cycle at 44,100 Hz = 441 Hz tone
    // Keep amplitude modest (64 to 192, centered at 128) so the HW-104 doesn't clip
    constexpr size_t kTestChunkSize = 1024; // 10 cycles per write (1000 bytes)
    constexpr size_t kCycleSamples = 64;

    for (size_t i = 0; i < kTestChunkSize; ++i) {
        size_t pos = i % kCycleSamples;
        if (pos < 32) {
            converted_audio[i] = static_cast<uint8_t>(64 + (pos * 128) / 32);
        } else {
            converted_audio[i] = static_cast<uint8_t>(192 - ((pos - 32) * 128) / 32);
        }
    }

    // Stream 132 chunks (~3 seconds of continuous audio at 44.1 kHz)
    for (int chunk = 0; chunk < 100; ++chunk) {
        esp_err_t err = write_audio_to_dac(converted_audio, kTestChunkSize);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "DAC test failed on chunk %d: %s", chunk, esp_err_to_name(err));
            return;
        }
    }

    ESP_LOGI(TAG, "Standalone DAC tone test finished successfully!");
}

} // namespace

void upsample_chime(const uint8_t* chime_data, size_t chime_size) {
    const int upsample_ratio = 4;
        
    // Small temporary buffer to hold the duplicated samples
    uint8_t buffer[256]; 
    size_t buf_idx = 0;
    
    // Use the explicit size passed into the function
    for (size_t i = 0; i < chime_size; i++) {
        uint8_t sample = chime_data[i];
        
        // Duplicate the sample 4 times
        for (int r = 0; r < upsample_ratio; r++) {
            buffer[buf_idx++] = sample;
            
            // When buffer is full, write to DAC and reset index
            if (buf_idx == sizeof(buffer)) {
                write_audio_to_dac(buffer, sizeof(buffer));
                buf_idx = 0;
            }
        }
    }
    
    // Flush any remaining samples in the buffer
    if (buf_idx > 0) {
        write_audio_to_dac(buffer, buf_idx);
    }
}

void play_chime(const char* sound_name)
{
    if (strcmp(sound_name, "connect") == 0) {
        ESP_LOGI(TAG, "Trying to play connect_raw");
        // sizeof() works correctly here because connect_raw is still recognized as an array
        upsample_chime(connect_raw, sizeof(connect_raw));
        
    } else if (strcmp(sound_name, "disconnect") == 0) {
        ESP_LOGI(TAG, "Trying to play disconnect_raw");
        upsample_chime(disconnect_raw, sizeof(disconnect_raw));
    }
}

extern "C" void app_main()
{
    s_ringbuf = xRingbufferCreate(16384, RINGBUF_TYPE_NOSPLIT);
    if (s_ringbuf == nullptr) {
        ESP_LOGE(TAG, "Failed to create audio ringbuffer");
        return;
    }

    xTaskCreate(
        audio_playback_task, 
        "audio_task", 
        8192, 
        nullptr, 
        5, 
        nullptr
    );
    
    ESP_ERROR_CHECK(configure_dac());
    ESP_ERROR_CHECK(enable_dac_audio());
    
    ESP_ERROR_CHECK(bluetooth::initialize());
    ESP_ERROR_CHECK(bluetooth::start());
    ESP_ERROR_CHECK(bluetooth::register_audio_data_callback(on_a2dp_audio_data));

    controls::init_hardware();

    vTaskDelay(pdMS_TO_TICKS(60000));
    
    // Testing for DAC output
    // test_dac_output();

    // Testing for bluetooth commands
    // bluetooth::send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_FORWARD, 0);
    // vTaskDelay(pdMS_TO_TICKS(50));
    // bluetooth::send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_FORWARD, 1);

    // Testing for chimes
    // play_chime("connect");

    ESP_LOGI(TAG, "A2DP-to-DAC audio pipeline started");
}