#pragma once

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "mdns.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef const char* string;
#define NoReturn void __attribute__((noreturn));

/* Function prototype */

/**
 * @brief initialize ota and calls required api to start ota
 * 
 * @param void nothing to pass as args
 * 
 * @return void nothing to return
 */
void init_ota(void);

/**
 * @brief a function which handles fatale errors & prints useful logs
 * 
 * @param exit_msg this message shows the main reason why there was an error
 * @param tag used for log tag
 * @param err used for knowing why this error happened pass in 0x101 prints out in string ESP_ERR_NO_MEM 
 * @param reset used for reseting the chip if true the chip gets reset
 **/
NoReturn task_fatal_error( string exit_msg, string tag, esp_err_t err, bool reset);

/* MACRO */

/**
 * @brief if this macro is called then the chip resets after printing the error msg.
 *
 * @param exit_msg this message shows the main reason why there was an error.
 * @param tag used for log tag.
 * @param err used for knowing why the error happened,
 *            eg:- if passed in 0x101, then it prints out a string ESP_ERR_NO_MEM 
 * 
 * @param reset used for reseting the chip if true the chip gets reset,
 *              always true in this case
 **/
#define TASK_ERROR_FATALE(exit_msg, tag, err)\
            task_fatal_error(exit_msg, tag, err, true);

/**
 * @brief if this macro is called then the chip does not resets after printing the error msg.
 *
 * @param exit_msg this message shows the main reason why there was an error.
 * @param tag used for log tag.
 * @param err used for knowing why the error happened.
 *            eg:- if passed in 0x101, then it prints out a string ESP_ERR_NO_MEM 
 * 
 * @param reset used for reseting the chip if true the chip gets reset,
 *              always false in this case.
 **/
#define TASK_ERROR_NON_FATALE(exit_msg, tag, err)\
            task_fatal_error(exit_msg, tag, err, false);



#ifdef __cplusplus
}
#endif