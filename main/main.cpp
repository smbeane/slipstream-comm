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

// Access the configuration flags defined in bluetooth.cpp
extern bool use_a2dp;
extern uint32_t input_freq;

namespace {

constexpr const char *TAG = "audio_pipeline";
constexpr size_t kConvertedBufferSize = 4096;

uint8_t converted_audio[kConvertedBufferSize];
RingbufHandle_t s_ringbuf = nullptr;

size_t convert_pcm_to_u8(const uint8_t *pcm, size_t pcm_size, int num_channels)
{
    ESP_LOGI(TAG, "Use A2DP: %d, Input Frequency: %d", use_a2dp, input_freq);
    const size_t frame_count = pcm_size / (num_channels * 2); 
    const int16_t *samples = reinterpret_cast<const int16_t *>(pcm);
    static int32_t quant_error = 0;
    
    static int phase_error = 0;
    size_t out_idx = 0; 

    for (size_t frame = 0; frame < frame_count; ++frame) {
        int32_t mono = 0;
        
        if (num_channels == 2) {
            int32_t left  = samples[frame * 2];
            int32_t right = samples[frame * 2 + 1];
            mono = (left + right) >> 1; 
        } else {
            mono = samples[frame];      
        }

        int32_t u16_with_err = mono + 32768 + quant_error;
        if (u16_with_err > 65535) u16_with_err = 65535;
        if (u16_with_err < 0)     u16_with_err = 0;

        uint8_t out_sample = static_cast<uint8_t>(u16_with_err >> 8);
        
        switch (input_freq) {
            case (8000): {
                converted_audio[out_idx++] = out_sample;
                converted_audio[out_idx++] = out_sample;
                converted_audio[out_idx++] = out_sample;
                converted_audio[out_idx++] = out_sample;
                converted_audio[out_idx++] = out_sample;
                converted_audio[out_idx++] = out_sample;
                break;
            }
            case (16000): {
                converted_audio[out_idx++] = out_sample;
                converted_audio[out_idx++] = out_sample;
                converted_audio[out_idx++] = out_sample;
                break;
            }
            case (44100): {
                converted_audio[out_idx++] = out_sample;
                
                phase_error += 13; // 160 - 147 = 13
                
                if (phase_error >= 147) {
                    converted_audio[out_idx++] = out_sample;
                    phase_error -= 147; 
                }
                break;
            }
            case (48000): {
                converted_audio[out_idx++] = out_sample;
                break;
            }
            default: {
                converted_audio[out_idx++] = out_sample;
                break;
            }
        }
        
        int32_t reconstructed = static_cast<int32_t>(out_sample) << 8;
        quant_error = u16_with_err - reconstructed;
    }

    return out_idx;
}

void on_audio_data(const uint8_t *pcm, size_t pcm_size)
{
    if (pcm == nullptr || pcm_size == 0 || s_ringbuf == nullptr) {
        return;
    }

    if (xRingbufferSend(s_ringbuf, pcm, pcm_size, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Audio ring buffer full; dropped %zu-byte packet", pcm_size);
    }
}

extern int current_a2dp_sample_rate; // Pull this from a2dp.cpp

void convert_441_to_480_test(const uint8_t *pcm, size_t pcm_size)
{
    // A2DP is stereo, so each frame is 4 bytes (Left 16-bit, Right 16-bit)
    const size_t frame_count = pcm_size / 4; 
    const int16_t *samples = reinterpret_cast<const int16_t *>(pcm);
    
    // We use a static error accumulator to maintain the fractional math across ringbuffer chunks
    static int phase_error = 0; 
    size_t out_idx = 0;

    for (size_t frame = 0; frame < frame_count; ++frame) {
        // 1. Convert to Mono and format for 8-bit DAC (your existing logic)
        int32_t left  = samples[frame * 2];
        int32_t right = samples[frame * 2 + 1];
        int32_t mono = (left + right) >> 1;
        
        int32_t u16_val = mono + 32768;
        if (u16_val > 65535) u16_val = 65535;
        if (u16_val < 0)     u16_val = 0;
        uint8_t out_sample = static_cast<uint8_t>(u16_val >> 8);

        // 2. Always output the real sample
        converted_audio[out_idx++] = out_sample;

        // 3. The Fractional Math (160 / 147)
        // Add the difference (160 - 147 = 13) to our error counter
        phase_error += 13; 
        
        // When the error overflows 147, it's time to insert our extra sample
        // This triggers roughly every 11.3 samples
        if (phase_error >= 147) {
            // Insert a duplicate of the sample we just wrote to stretch the audio
            converted_audio[out_idx++] = out_sample;
            phase_error -= 147; 
        }
    }
    
    // Write the stretched audio to the DAC (which must be running at 48000 Hz)
    write_audio_to_dac(converted_audio, out_idx);
}

void audio_playback_task(void *pvParameters)
{
    size_t item_size;
    
    // Determine channel count based on active profile for Phase 1 isolation
    
    while (true) {
        
        
        uint8_t *pcm = static_cast<uint8_t *>(
            xRingbufferReceive(s_ringbuf, &item_size, portMAX_DELAY)
        );
        
        if (pcm == nullptr) continue;
        
        const int num_channels = use_a2dp ? 2 : 1;
        const size_t bytes_per_frame = num_channels * 2;
        const size_t upsampling = use_a2dp ? 1 : 4;

        const size_t frame_count = item_size / bytes_per_frame;
        
        if (item_size % bytes_per_frame != 0 || frame_count > kConvertedBufferSize) {
            ESP_LOGW(TAG, "Skipping unsupported PCM buffer size: %zu bytes", item_size);
        } else {
            size_t audio_size = convert_pcm_to_u8(pcm, item_size, num_channels);
            
            esp_err_t err = write_audio_to_dac(converted_audio, audio_size);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "DAC write failed: %s", esp_err_to_name(err));
            }
        }

        vRingbufferReturnItem(s_ringbuf, pcm);
    }
}

void test_dac_output()
{
    ESP_LOGI(TAG, "Starting standalone 441Hz DAC tone test...");

    constexpr size_t kTestChunkSize = 1024;
    constexpr size_t kCycleSamples = 64;

    for (size_t i = 0; i < kTestChunkSize; ++i) {
        size_t pos = i % kCycleSamples;
        if (pos < 32) {
            converted_audio[i] = static_cast<uint8_t>(64 + (pos * 128) / 32);
        } else {
            converted_audio[i] = static_cast<uint8_t>(192 - ((pos - 32) * 128) / 32);
        }
    }

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
        
    uint8_t buffer[256]; 
    size_t buf_idx = 0;
    
    for (size_t i = 0; i < chime_size; i++) {
        uint8_t sample = chime_data[i];
        
        for (int r = 0; r < upsample_ratio; r++) {
            buffer[buf_idx++] = sample;
            
            if (buf_idx == sizeof(buffer)) {
                write_audio_to_dac(buffer, sizeof(buffer));
                buf_idx = 0;
            }
        }
    }
    
    if (buf_idx > 0) {
        write_audio_to_dac(buffer, buf_idx);
    }
}

void play_chime(const char* sound_name)
{
    if (strcmp(sound_name, "connect") == 0) {
        upsample_chime(connect_raw, sizeof(connect_raw));
        
    } else if (strcmp(sound_name, "disconnect") == 0) {
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
    // ESP_ERROR_CHECK(bluetooth::register_audio_data_callback(convert_441_to_480_test));
    ESP_ERROR_CHECK(bluetooth::register_audio_data_callback(on_audio_data));
    controls::init_hardware();

    vTaskDelay(pdMS_TO_TICKS(60000));
    
    ESP_LOGI(TAG, "Bluetooth-to-DAC audio pipeline started");
}