//
// Created by Melanie on 25/02/2025.
//

//#include <esp_vfs.h>
#include <sys/param.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_https_server.h>
#include "SimpleWebServer.h"
#include "main.h"
#include "ApiHttpRequest.h"
#include "ApiServer.h"

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

static const char *TAG = "simplewebserver";

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE   (200*1024) // 200 KB
#define MAX_FILE_SIZE_STR "200KB"

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/* Set HTTP response content type according to file extension */
esp_err_t SimpleWebServer::set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf")) {
        return httpd_resp_set_type(req, "application/pdf");
    } else if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".js")) {
        return httpd_resp_set_type(req, "text/javascript");
    } else if (IS_FILE_EXT(filename, ".css")) {
        return httpd_resp_set_type(req, "text/css");
    } else if (IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    } else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
 const char* SimpleWebServer::get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize) {
        /* Full path string won't fit into destination buffer */
        return nullptr;
    }

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

/* Handler to download a file kept on the server */
esp_err_t SimpleWebServer::download_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = nullptr;
    struct stat file_stat{};

    if (!strcmp(req->uri, "/"))
        strcpy((char *) req->uri, "/index.html");
    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri, sizeof(filepath));
    if (!filename) {
        ESP_LOGE(TAG, "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

//    /* If name has trailing '/', respond with directory contents */
//    if (filename[strlen(filename) - 1] == '/') {
//        return http_resp_dir_html(req, filepath);
//    }

    if (stat(filepath, &file_stat) == -1) {
        /* If file not present on SPIFFS check if URI
         * corresponds to one of the hardcoded paths */
//        if (strcmp(filename, "/index.html") == 0) {
//            return index_html_get_handler(req);
//        } else if (strcmp(filename, "/favicon.ico") == 0) {
//            return favicon_get_handler(req);
//        }
        ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
    set_content_type_from_file(req, filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
    size_t chunksize;
    do {
        /* Read file in chunks into the scratch buffer */
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

        if (chunksize > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                fclose(fd);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, nullptr);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }

        /* Keep looping till the whole file is sent */
    } while (chunksize != 0);

    /* Close file after sending complete */
    fclose(fd);
    ESP_LOGI(TAG, "File sending complete");

    /* Respond with an empty chunk to signal HTTP response completion */
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_send_chunk(req, nullptr, 0);
    return ESP_OK;
}

esp_err_t SimpleWebServer::root_get_handler(httpd_req_t *req)
{
    char host[64];
    esp_err_t ret = httpd_req_get_hdr_value_str(req, "host", host, 64);

    if (ret == ESP_OK)
        printf("Host header: %s\r\n", host);
    printf("URL: %s\r\n", req->uri);
    printf("Method: %d\r\n", req->method);

    static char fullhost[128];
    strcpy(fullhost, appMain.hostname().c_str());
    strcat(fullhost, ".net");

    ApiHttpRequest req2(req);
    printf("Body: %s\r\n", req2.body().c_str());

    auto resp = ApiServer::RequestHandler(req2);
    if (resp.statusCode() != 404)
    {
        resp.sendResponse(req);
        return ESP_OK;
    }

    if (req->method != HTTP_GET)
    {
        httpd_resp_send_404(req);
        return ESP_OK;
    }

    // This is what pops up the captive portal
    printf("Fullhost: %s Host: %s\r\n", fullhost, host);
    if (strcasecmp(fullhost, host) && !appMain.stationConnected())
    {
        sprintf(fullhost, "http://%s.net/index.html", appMain.hostname().c_str());
        httpd_resp_set_hdr(req, "Location", fullhost);
        httpd_resp_send_custom_err(req, "302", "Moved temporarily");
        return ESP_OK;
    }

    return download_get_handler(req);

//    httpd_resp_set_type(req, "text/html");
//    httpd_resp_send(req, "<h1>Hello Secure World!</h1>", HTTPD_RESP_USE_STRLEN);
//
//    return ESP_OK;
}

httpd_handle_t SimpleWebServer::start_webserver(const char *basePath)
{
    httpd_handle_t server = nullptr;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server");

    static struct file_server_data *server_data = nullptr;

    /* Allocate memory for server data */
    if (!server_data)
    {
        server_data = (file_server_data *) calloc(1, sizeof(struct file_server_data));
    }

    if (!server_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for server data");
        return nullptr;
    }

    strlcpy(server_data->base_path, basePath,
            sizeof(server_data->base_path));


    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
    conf.transport_mode = HTTPD_SSL_TRANSPORT_INSECURE;
    conf.httpd.uri_match_fn = httpd_uri_match_wildcard;


#if CONFIG_EXAMPLE_ENABLE_HTTPS_USER_CALLBACK
    conf.user_cb = https_server_user_callback;
#endif
    esp_err_t ret = httpd_ssl_start(&server, &conf);
    if (ESP_OK != ret) {
        ESP_LOGI(TAG, "Error starting server!");
        return nullptr;
    }

    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    static const httpd_uri_t root = {
        .uri       = "/",
        .method    = (httpd_method_t) HTTP_ANY,
        .handler   = root_get_handler,
        .user_ctx  = server_data
    };

    static const httpd_uri_t wild = {
        .uri       = "/*",
        .method    = (httpd_method_t) HTTP_ANY,
        .handler   = root_get_handler,
        .user_ctx  = server_data
    };

    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &wild);
    return server;
}

esp_err_t SimpleWebServer::stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    return httpd_ssl_stop(server);
}
