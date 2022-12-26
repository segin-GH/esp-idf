#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include <mdns.h>
#include <esp_spiffs.h>
#include <nvs_flash.h>
#include <esp_ota_ops.h>
#include "esp_app_format.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "ota.h"
#include "wifi_connect.h"

/* A macro for logging  */
static const char *SERVER_TAG = "[SERVER]";
static const char *OTA_TAG = "[OTA]";

/* server handle */
static httpd_handle_t server = NULL;


/* a function which handles fatale errors and restarts the chip */
static void __attribute__((noreturn)) task_fatal_error(const char *exit_msg)
{
    ESP_LOGE(OTA_TAG, "%s",exit_msg);
    ESP_LOGE(OTA_TAG, "Restarting due to fatal error...");
    esp_restart();

    for(;;) { /* never get out of this loop */ }
}



/* event handler for the default URL */
static esp_err_t on_default_url(httpd_req_t *req)
{
    /* LOG URL of req */
    ESP_LOGI(SERVER_TAG, "URL %s: ", req->uri);

    /* config spiffs for file reading*/
    esp_vfs_spiffs_conf_t esp_vfs_spiffs_config = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_vfs_spiffs_register(&esp_vfs_spiffs_config);

    /* determine the file path based on the URL */
    char path[600];

    /* if default url load the html file from spiffs*/
    if(strcmp(req->uri, "/") ==0)
        strcpy(path, "/spiffs/index.html");
    else
        sprintf(path,"/spiffs%s",req->uri);

    /* determine the file extension */
    char *ext = strrchr(path,'.');

    /* Set the MINE type based on the file extension */
    if(strcmp(ext, ".css") == 0)
        httpd_resp_set_type(req, "text/css");

    if(strcmp(ext, ".js") == 0)
        httpd_resp_set_type(req, "text/javascript");

    if(strcmp(ext, ".png") == 0)
        httpd_resp_set_type(req, "image/png");

    FILE *file = fopen(path, "r");
    if(file == NULL)
    {
        /* if file does not open send 404 error */
        httpd_resp_send_404(req);
        esp_vfs_spiffs_unregister(NULL);
        return ESP_FAIL;
    }

    /* Read the file in chunks and send it to client */
    char lineRead[256];
    while(fgets(lineRead, sizeof(lineRead), file))
        httpd_resp_sendstr_chunk(req, lineRead);

    /* send NULL to say everything has been send */
    httpd_resp_sendstr_chunk(req, NULL);

    /* close the file and unregister the spiffs */
    fclose(file);
    esp_vfs_spiffs_unregister(NULL);
    return ESP_OK;
}

/* OTA update handler function */
static esp_err_t on_ota_update(httpd_req_t *req)
{

    /* Check if the request is a POST request */
    if (req->method != HTTP_POST) {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method not allowed");
        return ESP_FAIL;
    }
    esp_err_t err;
    
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *update_partition = NULL;

    ESP_LOGI(OTA_TAG, "Starting to handle partition");

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    if( configured != running)
    {
        // ESP_LOGE(OTA_TAG, "(This can happen if either the OTA boot data or 
        //     preferred boot image become corrupted somehow.)");
    }
    
    ESP_LOGI(OTA_TAG, "Running partition type %d subtype %d (offset 0x%08"PRIx32")",
            running->type, running->subtype, running->address);

    update_partition = esp_ota_get_next_update_partition(NULL);
    if(update_partition == NULL)
        task_fatal_error("Error getting OTA update partition");

    ESP_LOGI(OTA_TAG, "Writing to partition subtype %d at offset 0x%"PRIx32,
             update_partition->subtype, update_partition->address);

    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if(err != ESP_OK)
        task_fatal_error("Error starting OTA update");

    /* Read the firmware image data from the request body and write it to the OTA partition */

    char buf[128];
    int received = 0;
    while(received < req->content_len)
    {
        int ret = httpd_req_recv(req, buf, sizeof(buf));
        if (ret <= 0)
        {
            esp_ota_end(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Error receiving request body");
            task_fatal_error("Error receiving request body");
        }
        received += ret;
        err = esp_ota_write(ota_handle, (const void *)buf, ret);
        if(err != ESP_OK)
        {
            esp_ota_end(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Error writing to OTA partition");
            task_fatal_error("Error writing to OTA partition");
        }
    }
    
    /* Finalize the OTA update process */
    err = esp_ota_end(ota_handle);
    if(err != ESP_OK)
        task_fatal_error("Error finalizing OTA update");

    /* Set the OTA partition as the active partition */
    err = esp_ota_set_boot_partition(update_partition);
    if(err != ESP_OK)
        task_fatal_error("Error setting OTA partition as active");

    ESP_LOGI(OTA_TAG, "OTA update successfully");
    ESP_LOGI(OTA_TAG, "Restarting in 2 seconds.....");
    vTaskDelay(2000/portTICK_PERIOD_MS);
    esp_restart();

    return ESP_OK;
}

/* function to start mDNS service */
static void start_mdns_service()
{
    /* initialize mDNS */
    mdns_init();
    /* set host name */
    mdns_hostname_set("esp-server");        /* new addr will be http://esp-server.local/ */
    /* set instance name */
    mdns_instance_name_set("esp-server-hspl");
}

/* init server */
static void init_server()
{
    /* set server as default config */
    httpd_config_t serverConfig = HTTPD_DEFAULT_CONFIG();

    /* Set the URI matching function to use wildcards */
    serverConfig.uri_match_fn = httpd_uri_match_wildcard;

    /* start the server */
    httpd_start(&server, &serverConfig);

    /* Set up a handler for requests to the default URL */
    httpd_uri_t default_url = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = on_default_url
    };
    httpd_register_uri_handler(server, &default_url);

    /* Add a handler for OTA update requests */
    httpd_uri_t ota_update_url = {
        .uri = "/ota",
        .method = HTTP_POST,
        .handler = on_ota_update
    };
    httpd_register_uri_handler(server, &ota_update_url);
}

void init_ota(void)
{
    ESP_LOGI(OTA_TAG,"INVOKING OTA");
    nvs_flash_init();
    wifi_init();
    wifi_connect_sta("Segin", "2003sejin", 10000);
    start_mdns_service();
    init_server();
}