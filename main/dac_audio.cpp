#include "external_c_api.hpp"
#include "driver/dac_continuous.h"
#include "esp_check.h"

static bool IRAM_ATTR example_on_convert_done_callback(dac_continuous_handle_t handle, const dac_event_data_t *event, void *user_data)
{
    QueueHandle_t que = (QueueHandle_t)user_data;
    BaseType_t need_awoke = pdFALSE;
    /* When the queue is full, drop the oldest item */
    if (xQueueIsQueueFullFromISR(que)) {
        dac_event_data_t dummy;
        xQueueReceiveFromISR(que, &dummy, &need_awoke);
    }
    /* Send the event from callback */
    xQueueSendFromISR(que, event, &need_awoke);
    return need_awoke;
}

void write_data_async(dac_continuous_handle_t dac_handle, QueueHandle_t que) {
    
}

void configure_dac(void) {
    dac_continuous_handle_t dac_handle;
    dac_continuous_config_t cont_cfd = {
        .chan_mask = DAC_CHANNEL_MASK_ALL,
        .desc_num  = 4,
        .buf_size  = 2048,
        .freq_hz   = 40000,
        .offset    = 0,
        .clk_src   = DAC_DIGI_CLK_SRC_DEFAULT,
        .chan_mode = DAC_CHANNEL_MODE_SIMUL,
    };

    ESP_ERROR_CHECK(dac_continuous_new_channels(&cont_cfd, &dac_handle));

    QueueHandle_t que = xQueueCreate(10, sizeof(dac_event_data_t));
    assert(que);

    dac_event_callbacks_t cbs {
        .on_convert_done = example_on_convert_done_callback,
        .on_stop         = NULL
    };

    ESP_ERROR_CHECK(dac_continuous_register_event_callback(dac_handle, &cbs, que));
    ESP_ERROR_CHECK(dac_continuous_enable(dac_handle));
    ESP_LOGI("configure_dac", "DAC configuration successful!");

    ESP_ERROR_CHECK(dac_continuous_start_async_writing(dac_handle));

    write_data_async(dac_handle, que);
}