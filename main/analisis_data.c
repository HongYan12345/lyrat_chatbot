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
#include "board.h"





#include "audio_idf_version.h"

#include <stdlib.h>

#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "esp_http_client.h"

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 1024

static const char *TAG = "ANALISIS_DATA";


char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
char output_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
char *token = "";
char *revolve_api = "";



esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // If user_data buffer is configured, copy the response into the buffer
                if (evt->user_data) {
                    memcpy(evt->user_data + output_len, evt->data, evt->data_len);
                } else {
                    if (output_buffer == NULL) {
                        output_buffer = (char *) malloc(esp_http_client_get_content_length(evt->client));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    memcpy(output_buffer + output_len, evt->data, evt->data_len);
                }
                output_len += evt->data_len;
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
                // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
    }
    return ESP_OK;
}

static void http_get_temperatura_interior(int time, int range){
    int content_length = 0;
    char str[1024];
    memset(str, 0, sizeof(str));
    strcat(str, "Bearer ");
    strcat(str ,token);

    esp_http_client_config_t config = {
        .event_handler = _http_event_handler,
        .url = "https://thingsboard.cloud:443/api/plugins/telemetry/DEVICE/d5c06100-61d0-11ed-b28a-eb999599ab40/values/timeseries?keys=temperature",
        .buffer_size_tx = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // GET
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_http_client_set_header(client, "Authorization", str);
    
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    } else {
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0) {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
        } else {
            int data_read = esp_http_client_read_response(client, local_response_buffer, MAX_HTTP_OUTPUT_BUFFER);
            if (data_read >= 0) {
                ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
                cJSON* root = NULL;
                root = cJSON_Parse(local_response_buffer);
                cJSON* cjson_item = cJSON_GetObjectItem(root, "temperature");
                cJSON* cjson_results =  cJSON_GetArrayItem(cjson_item,0);
                cJSON* cjson_temperatura = cJSON_GetObjectItem(cjson_results,"value");
                revolve_api = cjson_temperatura->valuestring;
                ESP_LOGI(TAG, "[x] get temperatura finish : %s", revolve_api);
            } else {
                ESP_LOGE(TAG, "Failed to read response");
            }
        }
    }
    esp_http_client_cleanup(client);
}

static void http_get_humedad(int time, int range){
    int content_length = 0;
    char str[1024];
    memset(str, 0, sizeof(str));
    strcat(str, "Bearer ");
    strcat(str ,token);
    
    esp_http_client_config_t config = {
        .event_handler = _http_event_handler,
        .url = "https://thingsboard.cloud:443/api/plugins/telemetry/DEVICE/76317650-590e-11ec-a919-556e8dbef35c/values/timeseries?keys=Temperatura%20Interior",
        .buffer_size_tx = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // GET
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_http_client_set_header(client, "Authorization", str);
    
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    } else {
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0) {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
        } else {
            int data_read = esp_http_client_read_response(client, local_response_buffer, MAX_HTTP_OUTPUT_BUFFER);
            if (data_read >= 0) {
                ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
                cJSON* root = NULL;
                root = cJSON_Parse(local_response_buffer);
                cJSON* cjson_item = cJSON_GetObjectItem(root, "Temperatura Interior");
                cJSON* cjson_results =  cJSON_GetArrayItem(cjson_item,0);
                cJSON* cjson_temperatura = cJSON_GetObjectItem(cjson_results,"value");
                revolve_api = cjson_temperatura->valuestring;
                ESP_LOGI(TAG, "[x] get temperatura finish : %s", revolve_api);
            } else {
                ESP_LOGE(TAG, "Failed to read response");
            }
        }
    }
    esp_http_client_cleanup(client);
}

static void http_get_token()
{
    
    esp_http_client_config_t config = {
        .event_handler = _http_event_handler,
        .url = "https://thingsboard.cloud:443/api/auth/login",
        .user_data = local_response_buffer,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // POST
    const char *post_data = "{\"username\":\"hongyan.xie@alumnos.upm.es\", \"password\":\"19980816xhy\"}";
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    
    esp_err_t err = esp_http_client_perform(client);

    //esp_http_client_read(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
    //int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
    if (err == ESP_OK) {
            ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
            esp_http_client_get_status_code(client),
            esp_http_client_get_content_length(client));
            cJSON* root = NULL;
            root = cJSON_Parse(local_response_buffer);
            cJSON* cjson_token = cJSON_GetObjectItem(root,"token");
            token = cjson_token->valuestring;
            ESP_LOGI(TAG, "[x] get token finish");

    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
   
    esp_http_client_cleanup(client);
}


char *send_problema(const char *text, int *pos){
    ESP_LOGI(TAG, "[x] analisis data : %s", text);
    char *text_return = "";
    if(*pos == 0){
        ESP_LOGI(TAG, "[x] analisis...");
        *pos = 1;
        text_return = "Estas pregundado la temperatura?";
    }
    else if(*pos == 1 && *pos == 2){
        *pos = 3;
        text_return = "hoy, ayer,semana o mes?";
    }
    
    else if(*pos == 3){
        *pos = 4;
        text_return = "maximo, minimo o medio?";
    }
    else{
        return send_error();
    }
    
    return text_return;
}
char *send_text(int tem_or_hum, int time, int range){
    //根据三个值判断具体的查询数据
    http_get_token();
    char *text_return = "";
    char test_text[1024];
    if(tem_or_hum == 0){
        http_get_temperatura_interior(time, range);
        ESP_LOGI(TAG, "temperatura: %s", revolve_api);
        
        memset(test_text, 0, sizeof(test_text));
        strcat(test_text,"La temperatura de ahora es");
        strcat(test_text,revolve_api);
    }
    else if(tem_or_hum == 1){
        http_get_humedad(time, range);
        ESP_LOGI(TAG, "humedad: %s", revolve_api);
        static char humedad_text[1024];
        memset(test_text, 0, sizeof(test_text));
        strcat(test_text,"La humedad de ahora es ");
        strcat(test_text,revolve_api);
    }
    else{
        text_return = send_error();
    }


    if(time == 0){

    }
    else if(time == 1){

    }
    else if(time == 2){

    }
    else if(time == 3){

    }
    else{
            
    }


    ESP_LOGI(TAG, "text_return: %s ", test_text);
    text_return = test_text;
    
    return text_return;
}
char *send_error(){
    char *text_error = "no entiendo, pregunta otra vez";
    return text_error;

}

