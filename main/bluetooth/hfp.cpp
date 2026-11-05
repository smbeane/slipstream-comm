#include "hfp.hpp"
#include "bluetooth.hpp"
#include "bluetooth/a2dp.hpp"


#include <esp_log.h>
#include <inttypes.h>
#include <esp_hf_client_api.h>
#include <esp_avrc_api.h>


esp_bd_addr_t peer_addr{0};
extern uint32_t input_freq;
extern bool use_a2dp;

namespace {
    
    constexpr const char *TAG = "bluetooth_hfp";
    
    const char *c_hf_evt_str[] = {
        "CONNECTION_STATE_EVT",              /*!< connection state changed event */
        "AUDIO_STATE_EVT",                   /*!< audio connection state change event */
        "VR_STATE_CHANGE_EVT",                /*!< voice recognition state changed */
        "CALL_IND_EVT",                      /*!< call indication event */
        "CALL_SETUP_IND_EVT",                /*!< call setup indication event */
        "CALL_HELD_IND_EVT",                 /*!< call held indicator event */
        "NETWORK_STATE_EVT",                 /*!< network state change event */
        "SIGNAL_STRENGTH_IND_EVT",           /*!< signal strength indication event */
        "ROAMING_STATUS_IND_EVT",            /*!< roaming status indication event */
        "BATTERY_LEVEL_IND_EVT",             /*!< battery level indication event */
        "CURRENT_OPERATOR_EVT",              /*!< current operator name event */
        "RESP_AND_HOLD_EVT",                 /*!< response and hold event */
        "CLIP_EVT",                          /*!< Calling Line Identification notification event */
        "CALL_WAITING_EVT",                  /*!< call waiting notification */
        "CLCC_EVT",                          /*!< listing current calls event */
        "VOLUME_CONTROL_EVT",                /*!< audio volume control event */
        "AT_RESPONSE",                       /*!< audio volume control event */
        "SUBSCRIBER_INFO_EVT",               /*!< subscriber information event */
        "INBAND_RING_TONE_EVT",              /*!< in-band ring tone settings */
        "LAST_VOICE_TAG_NUMBER_EVT",         /*!< requested number from AG event */
        "RING_IND_EVT",                      /*!< ring indication event */
        "PKT_STAT_EVT",                      /*!< requested number of packet status event */
        "PROF_STATE_EVT",                    /*!< Indicate HF CLIENT init or deinit complete */
    };

    const char *c_connection_state_str[] = {
        "disconnected",
        "connecting",
        "connected",
        "slc_connected",
        "disconnecting",
    };

    // esp_hf_client_audio_state_t
    const char *c_audio_state_str[] = {
        "disconnected",
        "connecting",
        "connected",
        "connected_msbc",
    };

    /// esp_hf_vr_state_t
    const char *c_vr_state_str[] = {
        "disabled",
        "enabled",
    };

    // esp_hf_service_availability_status_t
    const char *c_service_availability_status_str[] = {
        "unavailable",
        "available",
    };

    // esp_hf_roaming_status_t
    const char *c_roaming_status_str[] = {
        "inactive",
        "active",
    };

    // esp_hf_client_call_state_t
    const char *c_call_str[] = {
        "NO call in progress",
        "call in progress",
    };

    // esp_hf_client_callsetup_t
    const char *c_call_setup_str[] = {
        "NONE",
        "INCOMING",
        "OUTGOING_DIALING",
        "OUTGOING_ALERTING"
    };

    // esp_hf_client_callheld_t
    const char *c_call_held_str[] = {
        "NONE held",
        "Held and Active",
        "Held",
    };

    // esp_hf_response_and_hold_status_t
    const char *c_resp_and_hold_str[] = {
        "HELD",
        "HELD ACCEPTED",
        "HELD REJECTED",
    };

    // esp_hf_client_call_direction_t
    const char *c_call_dir_str[] = {
        "outgoing",
        "incoming",
    };

    // esp_hf_client_call_state_t
    const char *c_call_state_str[] = {
        "active",
        "held",
        "dialing",
        "alerting",
        "incoming",
        "waiting",
        "held_by_resp_hold",
    };

    // esp_hf_current_call_mpty_type_t
    const char *c_call_mpty_type_str[] = {
        "single",
        "multi",
    };

    // esp_hf_volume_control_target_t
    const char *c_volume_control_target_str[] = {
        "SPEAKER",
        "MICROPHONE"
    };

    // esp_hf_at_response_code_t
    const char *c_at_response_code_str[] = {
        "OK",
        "ERROR"
        "ERR_NO_CARRIER",
        "ERR_BUSY",
        "ERR_NO_ANSWER",
        "ERR_DELAYED",
        "ERR_BLACKLILSTED",
        "ERR_CME",
    };

    // const char *c_subscriber_service_type_str[] = {
    //     [ESP_HF_SUBSCRIBER_SERVICE_TYPE_UNKNOWN] = "unknown",
    //     [ESP_HF_SUBSCRIBER_SERVICE_TYPE_VOICE]   = "voice",
    //     [ESP_HF_SUBSCRIBER_SERVICE_TYPE_FAX]     = "fax",
    // };

    // esp_hf_client_in_band_ring_state_t
    const char *c_inband_ring_state_str[] = {
        "NOT provided",
        "Provided",
    };


} // namespace


namespace hfp {

    void hfp_event_callback(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param) {
        ESP_LOGI(TAG, "HFP_CALLBACK");
        
        
        
        switch (event) {
        case ESP_HF_CLIENT_CONNECTION_STATE_EVT:
        {
            ESP_LOGI(TAG, "--connection state %s, peer feats 0x%x, chld_feats 0x%x",
                    c_connection_state_str[param->conn_stat.state],
                    param->conn_stat.peer_feat,
                    param->conn_stat.chld_feat);
            memcpy(peer_addr,param->conn_stat.remote_bda,ESP_BD_ADDR_LEN);
            if (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED) {
                
            } else if (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_CONNECTED) {
                a2dp::send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_PAUSE, 0);
                vTaskDelay(pdMS_TO_TICKS(50));
                a2dp::send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_PAUSE, 1);
                
                use_a2dp = false;
            } else if (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED) {
                
            }
            break;
            }

        case ESP_HF_CLIENT_AUDIO_STATE_EVT:
        {
            if (param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED) {
                a2dp::send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_PAUSE, 0);
                vTaskDelay(pdMS_TO_TICKS(50));
                a2dp::send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_PAUSE, 1);
                
                input_freq = 8000;
                use_a2dp = false;
                
            } else if (param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC) {
                a2dp::send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_PAUSE, 0);
                vTaskDelay(pdMS_TO_TICKS(50));
                a2dp::send_avrc_passthrough_cmd(ESP_AVRC_PT_CMD_PAUSE, 1);
                    
                input_freq = 16000;
                use_a2dp = false;
            } else if (param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_DISCONNECTED) {
                
                a2dp::enable_a2dp();
            }


            ESP_LOGI(TAG, "--audio state %s",
                    c_audio_state_str[param->audio_stat.state]);
            break;
        }
         case ESP_HF_CLIENT_BVRA_EVT:
        {
            ESP_LOGI(TAG, "--VR state %s",
                    c_vr_state_str[param->bvra.value]);
            break;
        }

        case ESP_HF_CLIENT_CIND_SERVICE_AVAILABILITY_EVT:
        {
            ESP_LOGI(TAG, "--NETWORK STATE %s",
                    c_service_availability_status_str[param->service_availability.status]);
            break;
        }

        case ESP_HF_CLIENT_CIND_ROAMING_STATUS_EVT:
        {
            ESP_LOGI(TAG, "--ROAMING: %s",
                    c_roaming_status_str[param->roaming.status]);
            break;
        }

        case ESP_HF_CLIENT_CIND_SIGNAL_STRENGTH_EVT:
        {
            ESP_LOGI(TAG, "-- signal strength: %d",
                    param->signal_strength.value);
            break;
        }

        case ESP_HF_CLIENT_CIND_BATTERY_LEVEL_EVT:
        {
            ESP_LOGI(TAG, "--battery level %d",
                    param->battery_level.value);
            break;
        }

        case ESP_HF_CLIENT_COPS_CURRENT_OPERATOR_EVT:
        {
            ESP_LOGI(TAG, "--operator name: %s",
                    param->cops.name);
            break;
        }

        case ESP_HF_CLIENT_CIND_CALL_EVT:
        {
            ESP_LOGI(TAG, "--Call indicator %s",
                    c_call_str[param->call.status]);
            break;
        }

        case ESP_HF_CLIENT_CIND_CALL_SETUP_EVT:
        {
            ESP_LOGI(TAG, "--Call setup indicator %s",
                    c_call_setup_str[param->call_setup.status]);
            break;
        }

        case ESP_HF_CLIENT_CIND_CALL_HELD_EVT:
        {
            ESP_LOGI(TAG, "--Call held indicator %s",
                    c_call_held_str[param->call_held.status]);
            break;
        }

        case ESP_HF_CLIENT_BTRH_EVT:
        {
            ESP_LOGI(TAG, "--response and hold %s",
                    c_resp_and_hold_str[param->btrh.status]);
            break;
        }

        case ESP_HF_CLIENT_CLIP_EVT:
        {
            ESP_LOGI(TAG, "--clip number %s",
                    (param->clip.number == NULL) ? "NULL" : (param->clip.number));
            break;
        }

        case ESP_HF_CLIENT_CCWA_EVT:
        {
            ESP_LOGI(TAG, "--call_waiting %s",
                    (param->ccwa.number == NULL) ? "NULL" : (param->ccwa.number));
            break;
        }

        case ESP_HF_CLIENT_CLCC_EVT:
        {
            ESP_LOGI(TAG, "--Current call: idx %d, dir %s, state %s, mpty %s, number %s",
                    param->clcc.idx,
                    c_call_dir_str[param->clcc.dir],
                    c_call_state_str[param->clcc.status],
                    c_call_mpty_type_str[param->clcc.mpty],
                    (param->clcc.number == NULL) ? "NULL" : (param->clcc.number));
            break;
        }

        case ESP_HF_CLIENT_VOLUME_CONTROL_EVT:
        {
            ESP_LOGI(TAG, "--volume_target: %s, volume %d",
                    c_volume_control_target_str[param->volume_control.type],
                    param->volume_control.volume);
            break;
        }

        case ESP_HF_CLIENT_AT_RESPONSE_EVT:
        {
            ESP_LOGI(TAG, "--AT response event, code %d, cme %d",
                    param->at_response.code, param->at_response.cme);
            break;
        }

        case ESP_HF_CLIENT_CNUM_EVT:
        {
            // ESP_LOGI(TAG, "--subscriber type %s, number %s",
            //         c_subscriber_service_type_str[param->cnum.type],
            //         (param->cnum.number == NULL) ? "NULL" : param->cnum.number);
            break;
        }

        case ESP_HF_CLIENT_BSIR_EVT:
        {
            ESP_LOGI(TAG, "--inband ring state %s",
                    c_inband_ring_state_str[param->bsir.state]);
            break;
        }

        case ESP_HF_CLIENT_BINP_EVT:
        {
            ESP_LOGI(TAG, "--last voice tag number: %s",
                    (param->binp.number == NULL) ? "NULL" : param->binp.number);
            break;
        }
        case ESP_HF_CLIENT_PKT_STAT_NUMS_GET_EVT:
        {
            ESP_LOGE(TAG, "ESP_HF_CLIENT_PKT_STAT_NUMS_GET_EVT: %d", event);
            break;
        }
        case ESP_HF_CLIENT_PROF_STATE_EVT:
        {
            if (ESP_HF_INIT_SUCCESS == param->prof_stat.state) {
                ESP_LOGI(TAG, "HF PROF STATE: Init Complete");
            } else if (ESP_HF_DEINIT_SUCCESS == param->prof_stat.state) {
                ESP_LOGI(TAG, "HF PROF STATE: Deinit Complete");
            } else {
                ESP_LOGE(TAG, "HF PROF STATE error: %d", param->prof_stat.state);
            }
            break;
        }
        default:
            ESP_LOGE(TAG, "HF_CLIENT EVT: %d", event);
            break;
        }
    }

    void hfp_audio_data_callback(esp_hf_sync_conn_hdl_t sync_conn_hdl, esp_hf_audio_buff_t *audio_buf, bool is_bad_frame) {
        // If A2DP is active, drop the HFP data and free the buffer immediately
        if (use_a2dp) {
            esp_hf_client_audio_buff_free(audio_buf);
            return;
        }

        // 1. INCOMING: Route the decoded mSBC/CVSD audio to the DAC ringbuffer
        if (bluetooth::bt_audio_data_callback != nullptr && !is_bad_frame) {
            bluetooth::bt_audio_data_callback(audio_buf->data, static_cast<size_t>(audio_buf->data_len));
        }

        // 2. OUTGOING: Overwrite the processed buffer with zeros for perfect silence
        memset(audio_buf->data, 0, audio_buf->data_len);
        
        // Send the recycled buffer back to the stack using the correct signature
        esp_err_t err = esp_hf_client_audio_data_send(sync_conn_hdl, audio_buf);

        // 3. CLEANUP: Only free the buffer if the stack rejected it (queue full/error)
        if (err != ESP_OK) {
            esp_hf_client_audio_buff_free(audio_buf); 
        }
    }

    void hfp_incoming_audio_callback(const uint8_t *buf, uint32_t len) {
        if (use_a2dp) return;

        static int log_cnt = 0;
        if (log_cnt++ % 50 == 0) {
            ESP_LOGI("HFP_DEBUG", "INCOMING AUDIO: %d bytes", len);
        }

        if (bluetooth::bt_audio_data_callback != nullptr) {
            bluetooth::bt_audio_data_callback(buf, len);
        }
    }

    // 2. Outgoing Audio (ESP32 -> Phone)
    uint32_t hfp_outgoing_data_callback(uint8_t *buf, uint32_t len) {
        if (use_a2dp) return 0;
        
        // Satisfy the SCO TX starvation with perfect silence
        memset(buf, 0, len);
        return len; // Tell the stack the buffer is full and ready to send
    }

    void init() {

    }

    void start() {
        ESP_LOGI(TAG, "Starting HFP");
        
        ESP_ERROR_CHECK(esp_hf_client_register_callback(hfp_event_callback));
        ESP_ERROR_CHECK(esp_hf_client_register_audio_data_callback(hfp_audio_data_callback));
        // ESP_ERROR_CHECK(esp_hf_client_register_data_callback(hfp_incoming_audio_callback, hfp_outgoing_data_callback));


        ESP_ERROR_CHECK(esp_hf_client_init());
    }

    void stop() {

    }

} // namespace hfp