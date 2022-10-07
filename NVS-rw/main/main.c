#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>

#define TAG "NVS"

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    nvs_handle nvshandle;
    ESP_ERROR_CHECK(nvs_open("store",NVS_READWRITE, &nvshandle));
    int32_t val = 0;
    esp_err_t result = nvs_get_i32(nvshandle,"val",&val);
    switch(result)
    {
        case ESP_ERR_NVS_NOT_FOUND:
            ESP_LOGE(TAG, "Value not sest yet");
            break;

        case ESP_OK:
            ESP_LOGI(TAG, "value is %d",val);
            break;
        default:
            ESP_LOGI(TAG, "ERROR (%s) opening NVS handle! \n", esp_err_to_name(result));
            break;
    }
    val++;
    ESP_ERROR_CHECK(nvs_set_i32(nvshandle,"val",val));
    ESP_ERROR_CHECK(nvs_commit(nvshandle));
    nvs_close(nvshandle);   
    
}
