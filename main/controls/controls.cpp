#include "controls.hpp"

#include <esp_log.h>
#include "bluetooth/bluetooth.hpp"

namespace bluetooth {
    extern esp_err_t send_avrc_passthrough_cmd(uint8_t cmd, bool is_pressed);
}
extern void play_chime(const char* sound_name);

namespace controls {

    constexpr const char *TAG = "controls";

    QueueHandle_t button_evt_queue = nullptr;

    constexpr gpio_num_t BTN_SKIP = GPIO_NUM_27;
    constexpr gpio_num_t BTN_PAUSE = GPIO_NUM_33;
    constexpr gpio_num_t BTN_PREV = GPIO_NUM_32; 

    static void IRAM_ATTR button_isr_handler(void* arg) {
    static uint32_t last_isr_time = 0;
    uint32_t current_time = xTaskGetTickCountFromISR();

    if (current_time - last_isr_time > pdMS_TO_TICKS(50)) {
        uint32_t gpio_num = reinterpret_cast<uint32_t>(arg);
        
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(button_evt_queue, &gpio_num, &xHigherPriorityTaskWoken);
        
        last_isr_time = current_time;
    }
}
    
    void button_handler(void* arg) {
        uint32_t io_num;
        while(true) {
            if (xQueueReceive(button_evt_queue, &io_num, portMAX_DELAY)) {
                vTaskDelay(pdMS_TO_TICKS(50));
                if (io_num == BTN_PAUSE) {
                    if (bluetooth::is_currently_playing) {
                        ESP_LOGI(TAG, "Pausing!!!");
                        bluetooth::send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_PAUSE, 0);
                        vTaskDelay(pdMS_TO_TICKS(50));
                        bluetooth::send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_PAUSE, 1);

                    } else {
                        ESP_LOGI(TAG, "Playing!!!");
                        bluetooth::send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_PLAY, 0);
                        vTaskDelay(pdMS_TO_TICKS(50));
                        bluetooth::send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_PLAY, 1);
                        
                    }
                } else if (io_num == BTN_SKIP) {
                    ESP_LOGI(TAG, "Skipping!!!");
                    bluetooth::send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_FORWARD, 0);
                    vTaskDelay(pdMS_TO_TICKS(50));
                    bluetooth::send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_FORWARD, 1);
                } else if (io_num == BTN_PREV) {
                    ESP_LOGI(TAG, "Previous!!!");
                    bluetooth::send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_BACKWARD, 0);
                    vTaskDelay(pdMS_TO_TICKS(50));
                    bluetooth::send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_BACKWARD, 1);
                }
            }
        }
    }
    
    void init_hardware() {
        gpio_config_t io_conf = {};
    
        io_conf.intr_type = GPIO_INTR_NEGEDGE;
        io_conf.pin_bit_mask = (1ULL << BTN_SKIP) | (1ULL << BTN_PAUSE) | (1ULL << BTN_PREV);
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        gpio_config(&io_conf);
    
        button_evt_queue = xQueueCreate(10, sizeof(uint32_t));
        
        xTaskCreate(button_handler, "button_task", 4096, nullptr, 5, nullptr);
    
        gpio_install_isr_service(0);
        gpio_isr_handler_add(BTN_SKIP, button_isr_handler, (void*)BTN_SKIP);
        gpio_isr_handler_add(BTN_PAUSE, button_isr_handler, (void*)BTN_PAUSE);
        gpio_isr_handler_add(BTN_PREV, button_isr_handler, (void*)BTN_PREV);
    }
}