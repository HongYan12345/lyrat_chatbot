
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "esp_http_client.h"
#include "sdkconfig.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "board.h"
#include "esp_peripherals.h"
#include "periph_button.h"
#include "periph_wifi.h"
#include "periph_led.h"
#include "google_tts.h"
#include "google_sr.h"
#include "analisis_data.h"

#include "audio_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

#include "wake.c"

static const char *TAG = "LYRAT";

#define GOOGLE_SR_LANG "es-ES"            // https://cloud.google.com/speech-to-text/docs/languages
#define GOOGLE_TTS_LANG "es-ES"       //https://cloud.google.com/text-to-speech/docs/voices

#define EXAMPLE_RECORD_PLAYBACK_SAMPLE_RATE (16000)
audio_pipeline_handle_t pipeline = NULL;
esp_periph_handle_t led_handle = NULL;
google_sr_handle_t sr = NULL;
google_tts_handle_t tts = NULL;
bool is_wake = false;
audio_event_iface_handle_t evt = NULL;

//sr tts
void google_sr_begin(google_sr_handle_t sr)
{
    if (led_handle) {
        periph_led_blink(led_handle, get_green_led_gpio(), 500, 500, true, -1, 0);
    }
    
    ESP_LOGW(TAG, "Start speaking now");
}

//wake up
enum _rec_msg_id {
    REC_START = 1,
    REC_STOP,
    REC_CANCEL,
};

static char *TAG2 = "wwe_example";

static audio_rec_handle_t     recorder      = NULL;
static audio_element_handle_t raw_read      = NULL;
static QueueHandle_t          rec_q         = NULL;
static bool                   voice_reading = false;
static bool is_finish = false;

static esp_err_t rec_engine_cb(audio_rec_evt_t type, void *user_data)
{

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
    ESP_ERROR_CHECK(esp_netif_init());
#else
    tcpip_adapter_init();
#endif
    int a = 0;
    int *pos = &a;
    int tem_or_hum = 0;
    int time = 0;
    int range = 0;
    char *text = "";
    //audio_event_iface_msg_t msg;
    if (AUDIO_REC_WAKEUP_START == type) {
        ESP_LOGI(TAG2, "rec_engine_cb - REC_EVENT_WAKEUP_START");
        is_wake=true;
        //audio_recorder_wakenet_enable(recorder, false);
        
        google_tts_start(tts, "hola, soy demo", GOOGLE_TTS_LANG);
        //vTaskDelay(800);
        while(1){
            audio_event_iface_msg_t msg;
            audio_event_iface_listen(evt, &msg, portMAX_DELAY);
            ESP_LOGI(TAG, "[ * ] Event received: src_type:%d, source:%p cmd:%d, data:%p, data_len:%d",
                 msg.source_type, msg.source, msg.cmd, msg.data, msg.data_len);

            ESP_LOGI(TAG2, "tts..................%d", google_tts_check_event_finish(tts, &msg));
            if(google_tts_check_event_finish(tts, &msg)){
                ESP_LOGI(TAG2, "tts finish");
                break;
            }
        }
        

    } else if (AUDIO_REC_VAD_START == type) {
        ESP_LOGI(TAG2, "rec_engine_cb - REC_EVENT_VAD_START");
        if (!voice_reading) {
            int msg2 = REC_START;
            if (xQueueSend(rec_q, &msg2, 0) != pdPASS) {
                ESP_LOGE(TAG2, "rec start send failed");
            }
        }
        
        google_tts_stop(tts);
        ESP_LOGI(TAG, "[ * ] Resuming pipeline");
        google_sr_start(sr);
        
        
        while (1) {
            ESP_LOGI(TAG, "[ * ] Start sr");
            audio_event_iface_msg_t msg;
        
        if (audio_event_iface_listen(evt, &msg, portMAX_DELAY) != ESP_OK) {
            ESP_LOGW(TAG, "[ * ] Event process failed: src_type:%d, source:%p cmd:%d, data:%p, data_len:%d",
                     msg.source_type, msg.source, msg.cmd, msg.data, msg.data_len);
            continue;
        }

        ESP_LOGI(TAG, "[ * ] Event received: src_type:%d, source:%p cmd:%d, data:%p, data_len:%d",
                 msg.source_type, msg.source, msg.cmd, msg.data, msg.data_len);

        if (google_tts_check_event_finish(tts, &msg)) {
            ESP_LOGI(TAG, "[ * ] TTS Finish");
            continue;
        }

            google_tts_stop(tts);
            ESP_LOGI(TAG, "[ * ] Resuming pipeline");
            google_sr_start(sr);
            ESP_LOGI(TAG, "[ * ] Stop pipeline");
            //periph_led_stop(led_handle, get_green_led_gpio());

            char *original_text = google_sr_stop(sr);
        /*
            if (original_text == NULL) {
                continue;
            }
            ESP_LOGI(TAG, "Original text = %s", original_text);
            ESP_LOGI(TAG, "pos = %d", *pos);
            if(*pos == 0){
                text = send_problema(original_text, pos);
            }
            else if(*pos == 1){
                ESP_LOGI(TAG, "[ * ] temperatura");
                if(strcmp(original_text, "si")){
                    tem_or_hum = 1;
                    text = send_problema(original_text, pos);
                }
                else{
                    text = send_error();
                }
            }
            else if(*pos == 2){ 
                ESP_LOGI(TAG, "[ * ] humedad");
                if(strcmp(original_text, "si")){
                    tem_or_hum = 2;
                    text = send_problema(original_text, pos);
                }
                else{
                    text = send_error();
                }
            }
            else if(*pos == 3){
                ESP_LOGI(TAG, "[ * ] hoy, ayer, semana, mes");
                if(strcmp(original_text, "hoy")){
                    time = 1;
                }
                else if(strcmp(original_text, "ayer")){
                    time = 2;
                }
                else if(strcmp(original_text, "semana")){
                    time = 3;
                }
                else if(strcmp(original_text, "mes")){
                    time = 4;
                }
                
                if(time == 0){
                    text = send_error();
                }
                else{
                    text = send_problema(original_text, pos);
                }
            }
            else if(*pos == 4){
                ESP_LOGI(TAG, "[ * ] max, min ,medio");
                if(strcmp(original_text, "maximo")){
                range = 1;
                }
                else if(strcmp(original_text, "minimo")){
                    range = 2;
                }
                else if(strcmp(original_text, "medio")){
                    range = 3;
                }
                if(range == 0){
                    text = send_error();
                }
                else{
                    text = send_text(tem_or_hum, time, range);
                }
            }
            else{
                ESP_LOGI(TAG, "[ * ] error");
                text = send_error();
            }*/
            ESP_LOGI(TAG, "[ * ] next");
            google_tts_start(tts, text, GOOGLE_TTS_LANG);

        }
        ESP_LOGI(TAG, "[ 6 ] Stop audio_pipeline");

    } else if (AUDIO_REC_VAD_END == type) {
        ESP_LOGI(TAG2, "rec_engine_cb - REC_EVENT_VAD_STOP");
        
        //char *original_text = google_sr_stop(sr);
        //ESP_LOGE(TAG, "text : %s", original_text);
        if (voice_reading) {
            int msg2 = REC_STOP;
            if (xQueueSend(rec_q, &msg2, 0) != pdPASS) {
                ESP_LOGE(TAG2, "rec stop send failed");
            }
        }

    } else if (AUDIO_REC_WAKEUP_END == type) {
        ESP_LOGI(TAG2, "rec_engine_cb - REC_EVENT_WAKEUP_END");
        google_tts_stop(tts);
        google_sr_stop(sr);
        //is_finish = true;
        if(!is_finish){
            google_tts_start(tts, "no entiendo, llama me otra vez", GOOGLE_TTS_LANG);
            vTaskDelay(1000);
        }
        esp_restart();
    } else {
        ESP_LOGE(TAG2, "Unkown event");
    }
    return ESP_OK;
}

static int input_cb_for_afe(int16_t *buffer, int buf_sz, void *user_ctx, TickType_t ticks)
{
    return raw_stream_read(raw_read, (char *)buffer, buf_sz);
}

static void start_recorder()
{
#ifdef CONFIG_MODEL_IN_SPIFFS
    srmodel_spiffs_init();
#endif
    audio_element_handle_t i2s_stream_reader;
    
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    if (NULL == pipeline) {
        return;
    }

    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
#if RECORDER_SAMPLE_RATE == (16000)
    i2s_cfg.i2s_port = 1;
    i2s_cfg.i2s_config.use_apll = 0;
#endif
    i2s_cfg.i2s_config.sample_rate = RECORDER_SAMPLE_RATE;
    i2s_cfg.i2s_config.bits_per_sample = BITS_PER_SAMPLE;
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);

    audio_element_handle_t filter = NULL;
#if RECORDER_SAMPLE_RATE == (48000)
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = 48000;
    rsp_cfg.dest_rate = 16000;
    filter = rsp_filter_init(&rsp_cfg);
#endif

    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_READER;
    raw_read = raw_stream_init(&raw_cfg);

    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
    audio_pipeline_register(pipeline, raw_read, "raw");

    if (filter) {
        audio_pipeline_register(pipeline, filter, "filter");
        const char *link_tag[3] = {"i2s", "filter", "raw"};
        audio_pipeline_link(pipeline, &link_tag[0], 3);
    } else {
        const char *link_tag[2] = {"i2s", "raw"};
        audio_pipeline_link(pipeline, &link_tag[0], 2);
    }

    audio_pipeline_run(pipeline);
    ESP_LOGI(TAG2, "Recorder has been created");

    recorder_sr_cfg_t recorder_sr_cfg = DEFAULT_RECORDER_SR_CFG();
    recorder_sr_cfg.afe_cfg.alloc_from_psram = 3;
    recorder_sr_cfg.afe_cfg.wakenet_init = WAKENET_ENABLE;
    recorder_sr_cfg.afe_cfg.aec_init = AEC_ENABLE;
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0))
    recorder_sr_cfg.input_order[0] = DAT_CH_REF0;
    recorder_sr_cfg.input_order[1] = DAT_CH_0;
#endif

#if RECORDER_ENC_ENABLE == (true)
    rsp_filter_cfg_t filter_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    filter_cfg.src_ch = 1;
    filter_cfg.src_rate = 16000;
    filter_cfg.dest_ch = 1;
    filter_cfg.dest_rate = 8000;
    filter_cfg.stack_in_ext = true;
    filter_cfg.max_indata_bytes = 1024;

    amrnb_encoder_cfg_t amrnb_cfg = DEFAULT_AMRNB_ENCODER_CONFIG();
    amrnb_cfg.contain_amrnb_header = true;
    amrnb_cfg.stack_in_ext = true;

    recorder_encoder_cfg_t recorder_encoder_cfg = { 0 };
    recorder_encoder_cfg.resample = rsp_filter_init(&filter_cfg);
    recorder_encoder_cfg.encoder = amrnb_encoder_init(&amrnb_cfg);
#endif
    audio_rec_cfg_t cfg = AUDIO_RECORDER_DEFAULT_CFG();
    cfg.read = (recorder_data_read_t)&input_cb_for_afe;
    cfg.sr_handle = recorder_sr_create(&recorder_sr_cfg, &cfg.sr_iface);
#if RECORDER_ENC_ENABLE == (true)
    cfg.encoder_handle = recorder_encoder_create(&recorder_encoder_cfg, &cfg.encoder_iface);
#endif
    cfg.event_cb = rec_engine_cb;
    cfg.vad_off = 1000;
    recorder = audio_recorder_create(&cfg);
}

static void log_clear(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    esp_log_level_set("AUDIO_THREAD", ESP_LOG_ERROR);
    esp_log_level_set("I2C_BUS", ESP_LOG_ERROR);
    esp_log_level_set("AUDIO_HAL", ESP_LOG_ERROR);
    esp_log_level_set("ESP_AUDIO_TASK", ESP_LOG_ERROR);
    esp_log_level_set("ESP_DECODER", ESP_LOG_ERROR);
    esp_log_level_set("I2S", ESP_LOG_ERROR);
    esp_log_level_set("AUDIO_FORGE", ESP_LOG_ERROR);
    esp_log_level_set("ESP_AUDIO_CTRL", ESP_LOG_ERROR);
    esp_log_level_set("AUDIO_PIPELINE", ESP_LOG_ERROR);
    esp_log_level_set("AUDIO_ELEMENT", ESP_LOG_ERROR);
    esp_log_level_set("TONE_PARTITION", ESP_LOG_ERROR);
    esp_log_level_set("TONE_STREAM", ESP_LOG_ERROR);
    esp_log_level_set("MP3_DECODER", ESP_LOG_ERROR);
    esp_log_level_set("I2S_STREAM", ESP_LOG_ERROR);
    esp_log_level_set("RSP_FILTER", ESP_LOG_ERROR);
    esp_log_level_set("AUDIO_EVT", ESP_LOG_ERROR);
}


void chatbot_task(void *pv)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
    ESP_ERROR_CHECK(esp_netif_init());
#else
    tcpip_adapter_init();
#endif

    ESP_LOGI(TAG, "[ 1 ] Initialize Connect to Wi-Fi network, ssid=%s", CONFIG_WIFI_SSID);
    // Initialize peripherals management
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    periph_wifi_cfg_t wifi_cfg = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
/*
    // Initialize Button peripheral
    periph_button_cfg_t btn_cfg = {
        .gpio_mask = (1ULL << get_input_mode_id()) | (1ULL << get_input_rec_id()),
    };
    esp_periph_handle_t button_handle = periph_button_init(&btn_cfg);

    periph_led_cfg_t led_cfg = {
        .led_speed_mode = LEDC_LOW_SPEED_MODE,
        .led_duty_resolution = LEDC_TIMER_10_BIT,
        .led_timer_num = LEDC_TIMER_0,
        .led_freq_hz = 5000,
    };
    led_handle = periph_led_init(&led_cfg);

*/
    // Start wifi & button peripheral
    //esp_periph_start(set, button_handle);
    esp_periph_start(set, wifi_handle);
    //esp_periph_start(set, led_handle);

    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);

    ESP_LOGI(TAG, "[ 2 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    AUDIO_MEM_SHOW(TAG);
    start_recorder();
    AUDIO_MEM_SHOW(TAG);
    google_sr_config_t sr_config = {
        .api_key = CONFIG_GOOGLE_API_KEY,
        .lang_code = GOOGLE_SR_LANG,
        .record_sample_rates = EXAMPLE_RECORD_PLAYBACK_SAMPLE_RATE,
        .encoding = ENCODING_LINEAR16,
        .on_begin = google_sr_begin,
    };
    sr = google_sr_init(&sr_config);

    google_tts_config_t tts_config = {
        .api_key = CONFIG_GOOGLE_API_KEY,
        .playback_sample_rate = EXAMPLE_RECORD_PLAYBACK_SAMPLE_RATE,
    };
    tts = google_tts_init(&tts_config);

    ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from the pipeline");
    google_sr_set_listener(sr, evt);
    google_tts_set_listener(tts, evt);

    ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGI(TAG, "[ 5 ] Listen for all pipeline events");

    int msg_2 = 0;
    TickType_t delay = portMAX_DELAY;
    AUDIO_MEM_SHOW(TAG);

    while (true) {
        if(is_finish){
            audio_recorder_destroy (recorder);
            audio_pipeline_stop(pipeline);
            audio_pipeline_wait_for_stop(pipeline);
            audio_pipeline_terminate(pipeline);
            audio_pipeline_unlink(pipeline);
            start_recorder();
            is_finish=false;
        }
        while(is_wake){
            //ESP_LOGI(TAG2, "wake up end");
        }
        if (xQueueReceive(rec_q, &msg_2, delay) == pdTRUE) {
            switch (msg_2) {
                case REC_START: {
                    ESP_LOGW(TAG2, "voice read begin");
                    delay = 0;
                    voice_reading = true;
                    /*
                    while(is_wake){
                        vTaskDelay(200);
            audio_recorder_destroy (recorder);
            audio_pipeline_stop(pipeline);
            audio_pipeline_wait_for_stop(pipeline);
            //audio_pipeline_unlink(pipeline);
            //audio_pipeline_deinit(pipeline);
            vTaskDelay(100);
        
            google_tts_start(tts, "hola, soy demo", GOOGLE_TTS_LANG);
            while(1){
                ESP_LOGW(TAG2, "tts...............");
            }
            
        }*/
        break;
                }
                case REC_STOP: {
                    ESP_LOGW(TAG2, "voice read stopped");
                    delay = portMAX_DELAY;
                    voice_reading = false;
                    is_wake=false;
                    break;
                }
                case REC_CANCEL: {
                    ESP_LOGW(TAG2, "voice read cancel");
                    delay = portMAX_DELAY;
                    voice_reading = false;
                    break;
                }
                default:
                    break;
            }
            
        }
        
    }
    audio_recorder_destroy (recorder);
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    google_sr_destroy(sr);
    google_tts_destroy(tts);
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    audio_event_iface_destroy(evt);
    esp_periph_set_destroy(set);
    vTaskDelete(NULL);

    
}



void app_main(void)
{
    log_clear();
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_INFO);
    rec_q = xQueueCreate(3, sizeof(int));
    
    audio_thread_create(NULL, "read_task", chatbot_task, NULL, 8 * 1024, 5, true, 0);
}