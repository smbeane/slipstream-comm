#include <cstdio>
#include "external_c_api.hpp"

#define BLINK_GPIO GPIO_NUM_5
#define BLINK_PERIOD 5000

static int led_state = 0;


void configure_led(void) {
    
    gpio_reset_pin(BLINK_GPIO);

    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

}

void blink_led(void) {
    gpio_set_level(BLINK_GPIO, led_state);
}

extern "C" void app_main(void) {

    ESP_LOGI("app_main", "Configuring LED!");
    configure_led();

    while(1) {
        blink_led();
        led_state = !led_state;


        vTaskDelay(BLINK_PERIOD / portTICK_PERIOD_MS);
    }
}
