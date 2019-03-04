/* Play music from Bluetooth device

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "audio_common.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "audio_pipeline.h"
#include "bluetooth_service.h"
#include "esp_log.h"
#include "esp_peripherals.h"
#include "wav_decoder.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "periph_spiffs.h"
#include "spiffs_stream.h"


#define SAVE_FILE_RATE      22050
#define SAVE_FILE_CHANNEL   1
#define SAVE_FILE_BITS      8

#define PLAYBACK_RATE       44100
#define PLAYBACK_CHANNEL    2
#define PLAYBACK_BITS       16

static const char *TAG = "BLUETOOTH_SOURCE_EXAMPLE";

static audio_element_handle_t create_spiffs_stream(int sample_rates, int bits, int channels, audio_stream_type_t type)
{
    spiffs_stream_cfg_t spiffs_cfg = SPIFFS_STREAM_CFG_DEFAULT();
    spiffs_cfg.type = type;
    audio_element_handle_t spiffs_stream = spiffs_stream_init(&spiffs_cfg);
    mem_assert(spiffs_stream);
    audio_element_info_t writer_info = {0};
    audio_element_getinfo(spiffs_stream, &writer_info);
    writer_info.bits = bits;
    writer_info.channels = channels;
    writer_info.sample_rates = sample_rates;
    audio_element_setinfo(spiffs_stream, &writer_info);
    return spiffs_stream;
}

void app_main(void)
{
    //nvs_flash_erase();
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }


    audio_pipeline_handle_t pipeline;
    audio_element_handle_t spiffs_reader_el, filter_upsample_el, wav_decoder, bt_stream_writer;

    // Initialize peripherals management
    esp_periph_config_t periph_cfg = { 0 };
    esp_periph_init(&periph_cfg);

    // Initialize Spiffs peripheral
    periph_spiffs_cfg_t spiffs_cfg = {
        .root = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_periph_handle_t spiffs_handle = periph_spiffs_init(&spiffs_cfg);

    // Start spiffs peripheral
    esp_periph_start(spiffs_handle);

    // Wait until spiffs was mounted
    while (!periph_spiffs_is_mounted(spiffs_handle)) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }



    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    ESP_LOGI(TAG, "[ 1 ] Create Bluetooth service");
    bluetooth_service_cfg_t bt_cfg = {
        .device_name = "ESP-ADF-SOURCE",
        .mode = BLUETOOTH_A2DP_SOURCE,
        .remote_name = "BT-12",
    };
    bluetooth_service_start(&bt_cfg);

    ESP_LOGI(TAG, "[1.1] Get Bluetooth stream");
    bt_stream_writer = bluetooth_service_create_stream();

    ESP_LOGI(TAG, "[ 2 ] Create SPIFFS stream to read data");
    spiffs_reader_el = create_spiffs_stream(SAVE_FILE_RATE, SAVE_FILE_BITS, SAVE_FILE_CHANNEL, AUDIO_STREAM_READER);


    ESP_LOGI(TAG, "[ 3 ] Create wav decoder to decode wav file");
    wav_decoder_cfg_t wavdec_cfg = DEFAULT_WAV_DECODER_CONFIG();
    wav_decoder = wav_decoder_init(&wavdec_cfg);
    //audio_element_set_read_cb(wav_decoder, wav_music_read_cb, NULL);

    ESP_LOGI(TAG, "[ 4 ] Create audio pipeline for BT Source");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(TAG, "[4.1] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, spiffs_reader_el,     "spiffs_reader");
    audio_pipeline_register(pipeline, wav_decoder,        "wav");
    audio_pipeline_register(pipeline, bt_stream_writer,   "bt");

    ESP_LOGI(TAG, "[4.2] Link it together wav_decoder-->bt_stream_writer");
    audio_pipeline_link(pipeline, (const char *[]) {"spiffs_reader", "wav", "bt"}, 3);

    audio_element_set_uri(spiffs_reader_el, "/spiffs/pcm1644s.wav");

    ESP_LOGI(TAG, "[5.1] Create Bluetooth peripheral");
    esp_periph_handle_t bt_periph = bluetooth_service_create_periph();

    ESP_LOGI(TAG, "[5.2] Start Bluetooth peripheral");
    esp_periph_start(bt_periph);
    ESP_LOGI(TAG, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");

    ESP_LOGI(TAG, "[ 6 ] Setup event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[6.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "[6.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_get_event_iface(), evt);

    ESP_LOGI(TAG, "[ 7 ] Start audio_pipeline");
    audio_pipeline_run(pipeline);

    ESP_LOGI(TAG, "[ 8 ] Listen for all pipeline events");
    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        ESP_LOGI(TAG, "[ * ] Audio event");
        ESP_LOGI(TAG, "[ * ] Free heap: %u", xPortGetFreeHeapSize());
        ESP_LOGI(TAG, "[ * ] msg.cmd: %d", msg.cmd);

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) wav_decoder
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = {0};
            audio_element_getinfo(wav_decoder, &music_info);

            ESP_LOGI(TAG, "[ * ] Receive music info from wav decoder, sample_rates=%d, bits=%d, ch=%d",
                     music_info.sample_rates, music_info.bits, music_info.channels);

            audio_element_setinfo(bt_stream_writer, &music_info);
            //i2s_stream_set_clk(bt_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
            continue;
        }


        /* Stop when the Bluetooth is disconnected or suspended */
        if (msg.source_type == PERIPH_ID_BLUETOOTH
                && msg.source == (void *)bt_periph) {
            if ((msg.cmd == PERIPH_BLUETOOTH_DISCONNECTED) || (msg.cmd == PERIPH_BLUETOOTH_AUDIO_SUSPENDED)) {
                ESP_LOGW(TAG, "[ * ] Bluetooth disconnected or suspended");
                periph_bluetooth_stop(bt_periph);
                break;
            }
        }
    }

    ESP_LOGI(TAG, "[ 9 ] Stop audio_pipeline");
    audio_pipeline_terminate(pipeline);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Stop all peripherals before removing the listener */
    esp_periph_stop_all();
    audio_event_iface_remove_listener(esp_periph_get_event_iface(), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_unregister(pipeline, spiffs_reader_el);
    audio_pipeline_unregister(pipeline, bt_stream_writer);
    audio_pipeline_unregister(pipeline, wav_decoder);
    //audio_pipeline_deinit(spiffs_reader_el);
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(bt_stream_writer);
    audio_element_deinit(wav_decoder);
    esp_periph_destroy();
    bluetooth_service_destroy();
}
