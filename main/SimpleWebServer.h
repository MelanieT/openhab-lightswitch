//
// Created by Melanie on 25/02/2025.
//

#ifndef LIGHTSWITCH_SIMPLEWEBSERVER_H
#define LIGHTSWITCH_SIMPLEWEBSERVER_H

/* Scratch buffer size */
#define SCRATCH_BUFSIZE  8192

#include "esp_vfs.h"
#include <esp_http_server.h>

class SimpleWebServer
{
private:
    struct file_server_data {
        /* Base path of file storage */
        char base_path[ESP_VFS_PATH_MAX + 1];

        /* Scratch buffer for temporary storage during file transfer */
        char scratch[SCRATCH_BUFSIZE];
    };
public:
    static httpd_handle_t start_webserver(const char *basePath);
    static esp_err_t stop_webserver(httpd_handle_t server);
private:
    static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename);
    static const char* get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize);
    static esp_err_t root_get_handler(httpd_req_t *req);
    static esp_err_t download_get_handler(httpd_req_t *req);
};


#endif //LIGHTSWITCH_SIMPLEWEBSERVER_H
