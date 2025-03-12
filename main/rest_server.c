
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "esp_err.h"
#include "esp_log.h"

#include "cJSON.h"
#include "esp_chip_info.h"
#include "esp_http_server.h"
#include "esp_random.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"

#include "led_plate.h"
#include "rest_server.h"

/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE (200 * 1024) // 200 KB
#define MAX_FILE_SIZE_STR "200KB"

/* Scratch buffer size */
#define SCRATCH_BUFSIZE 16384

#define REQ_CTX ((struct file_server_data*)(req->user_ctx))

struct file_server_data {
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];

    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};

static const char* TAG = "file_server";

uint8_t webOpenedCounter = 0;
uint8_t imageSetCounter = 0;
uint8_t projectSaveCounter = 0;

static esp_err_t index_html_get_handler(httpd_req_t* req)
{
    extern const unsigned char index_html_start[] asm("_binary_index_html_start");
    extern const unsigned char index_html_end[] asm("_binary_index_html_end");
    const size_t index_html_size = (index_html_end - index_html_start);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char*)index_html_start, index_html_size);
    return ESP_OK;
}

static esp_err_t favicon_get_handler(httpd_req_t* req)
{
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[] asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
    httpd_resp_send(req, (const char*)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}

static esp_err_t main_css_get_handler(httpd_req_t* req)
{
    extern const unsigned char main_css_start[] asm("_binary_main_css_gz_start");
    extern const unsigned char main_css_end[] asm("_binary_main_css_gz_end");
    const size_t main_css_size = (main_css_end - main_css_start);
    httpd_resp_set_type(req, "text/css");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
    httpd_resp_send(req, (const char*)main_css_start, main_css_size);
    return ESP_OK;
}

static esp_err_t bundle_js_get_handler(httpd_req_t* req)
{
    extern const unsigned char bundle_js_start[] asm("_binary_bundle_js_gz_start");
    extern const unsigned char bundle_js_end[] asm("_binary_bundle_js_gz_end");
    const size_t bundle_js_size = (bundle_js_end - bundle_js_start);
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
    httpd_resp_send(req, (const char*)bundle_js_start, bundle_js_size);
    return ESP_OK;
}

static esp_err_t apple_touch_icon_get_handler(httpd_req_t* req)
{
    extern const unsigned char apple_touch_icon_png_start[] asm("_binary_apple_touch_icon_png_start");
    extern const unsigned char apple_touch_icon_png_end[] asm("_binary_apple_touch_icon_png_end");
    const size_t apple_touch_icon_png_size = (apple_touch_icon_png_end - apple_touch_icon_png_start);
    httpd_resp_set_type(req, "image/png");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
    httpd_resp_send(req, (const char*)apple_touch_icon_png_start, apple_touch_icon_png_size);
    return ESP_OK;
}

#define IS_FILE_EXT(filename, ext) (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

static esp_err_t set_content_type_from_file(httpd_req_t* req, const char* filename)
{
    if (IS_FILE_EXT(filename, ".pdf")) {
        return httpd_resp_set_type(req, "application/pdf");
    } else if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    } else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

void set_resp_CORS_headers(httpd_req_t* req, const char * methods){
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "access-control-allow-origin,content-type");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", methods);
    httpd_resp_set_type(req, "text/plain");
}

/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
static const char* get_path_from_uri(char* dest, const char* base_path, const char* uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char* quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char* hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize) {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

static esp_err_t download_get_handler(httpd_req_t* req)
{
    char filepath[FILE_PATH_MAX];
    FILE* fd = NULL;
    struct stat file_stat;

    const char* filename = get_path_from_uri(filepath, REQ_CTX->base_path, req->uri, sizeof(filepath));
    if (!filename) {
        ESP_LOGE(TAG, "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* If name has trailing '/', respond with directory contents */
    if (filename[strlen(filename) - 1] == '/') {
        if (webOpenedCounter < 255)
            webOpenedCounter++;
        return index_html_get_handler(req);
    }

    if (stat(filepath, &file_stat) == -1) {
        /* If file not present on SPIFFS check if URI
         * corresponds to one of the hardcoded paths */
        if (strcmp(filename, "/index.html") == 0) {
            if (webOpenedCounter < 255)
                webOpenedCounter++;
            return index_html_get_handler(req);
        } else if (strcmp(filename, "/favicon.ico") == 0) {
            return favicon_get_handler(req);
        } else if (strcmp(filename, "/main.css") == 0 || strcmp(filename, "/css/main.css") == 0) {
            return main_css_get_handler(req);
        } else if (strcmp(filename, "/bundle.js") == 0) {
            return bundle_js_get_handler(req);
        } else if (strcmp(filename, "/apple-touch-icon.png") == 0) {
            return apple_touch_icon_get_handler(req);
        }
        ESP_LOGE(TAG, "Failed to stat file : %s", filepath);

        /* Captive portal redirect*/
        httpd_resp_set_status(req, "302 Temporary Redirect");
        httpd_resp_set_hdr(req, "Location", "http://badge.phd2/");
        // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
        httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);
        ESP_LOGI(TAG, "Redirecting to root");

        return ESP_OK;
    }

    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
    set_content_type_from_file(req, filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char* chunk = REQ_CTX->scratch;
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
                httpd_resp_sendstr_chunk(req, NULL);
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
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Handler to upload a file onto the server */
static esp_err_t upload_post_handler(httpd_req_t* req)
{
    char filepath[FILE_PATH_MAX];
    FILE* fd = NULL;
    struct stat file_stat;

    /* Skip leading "/upload" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char* filename = get_path_from_uri(filepath, REQ_CTX->base_path, req->uri + sizeof("/upload") - 1, sizeof(filepath));
    if (!filename) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == 0) {
        ESP_LOGE(TAG, "File already exists : %s", filepath);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
        return ESP_FAIL;
    }

    /* File cannot be larger than a limit */
    if (req->content_len > MAX_FILE_SIZE) {
        ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File size must be less than " MAX_FILE_SIZE_STR "!");
        /* Return failure to close underlying connection else the
         * incoming file content will keep the socket busy */
        return ESP_FAIL;
    }

    fd = fopen(filepath, "w");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to create file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Receiving file : %s...", filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char* buf = REQ_CTX->scratch;
    int received;

    /* Content length of the request gives
     * the size of the file being uploaded */
    int remaining = req->content_len;

    while (remaining > 0) {

        ESP_LOGI(TAG, "Remaining size : %d", remaining);
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry if timeout occurred */
                continue;
            }

            /* In case of unrecoverable error,
             * close and delete the unfinished file*/
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(TAG, "File reception failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        /* Write buffer content to file on storage */
        if (received && (received != fwrite(buf, 1, received, fd))) {
            /* Couldn't write everything to file!
             * Storage may be full? */
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(TAG, "File write failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            return ESP_FAIL;
        }

        /* Keep track of remaining size of
         * the file left to be uploaded */
        remaining -= received;
    }

    /* Close file upon upload completion */
    fclose(fd);
    ESP_LOGI(TAG, "File reception complete");

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_sendstr(req, "File uploaded successfully");
    return ESP_OK;
}

/* Handler to delete a file from the server */
static esp_err_t delete_post_handler(httpd_req_t* req)
{
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;

    /* Skip leading "/delete" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char* filename = get_path_from_uri(filepath, REQ_CTX->base_path, req->uri + sizeof("/delete") - 1, sizeof(filepath));
    if (!filename) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "File does not exist : %s", filename);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Deleting file : %s", filename);
    /* Delete file */
    unlink(filepath);

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_sendstr(req, "File deleted successfully");
    return ESP_OK;
}

static esp_err_t system_info_get_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON* root = cJSON_CreateObject();
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddStringToObject(root, "version", IDF_VER);
    cJSON_AddNumberToObject(root, "cores", chip_info.cores);
    const char* sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void*)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t set_picture_post_handler(httpd_req_t* req)
{
    updateLastActivity();
    if (imageSetCounter < 255)
        imageSetCounter++;
    int total_len = req->content_len;
    int cur_len = 0;
    char* buf = REQ_CTX->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post picture pixels");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON* root = cJSON_Parse(buf);
    cJSON* array = cJSON_GetObjectItem(root, "values");
    uint8_t imagePixels[300];

    for (int i = 0; i < cJSON_GetArraySize(array); i++) {
        imagePixels[i] = cJSON_GetArrayItem(array, i)->valueint;
    }
    updateImageToShow(imagePixels);
    cJSON_Delete(root);

    set_resp_CORS_headers(req, "POST");
    httpd_resp_sendstr(req, "Picture pixels setted successfully");
    return ESP_OK;
}

static esp_err_t set_picture_options_handler(httpd_req_t* req)
{
    set_resp_CORS_headers(req, "POST");
    httpd_resp_sendstr(req, "Picture pixels upload method");
    return ESP_OK;
}

static esp_err_t projects_post_handler(httpd_req_t* req)
{
    updateLastActivity();
    if (projectSaveCounter < 255)
        projectSaveCounter++;

    char filepath[FILE_PATH_MAX];
    FILE* fd = NULL;

    const char* filename = "/projects.json";

    const size_t base_pathlen = strlen(REQ_CTX->base_path);
    size_t pathlen = strlen(filename);
    strcpy(filepath, REQ_CTX->base_path);
    strlcpy(filepath + base_pathlen, filename, pathlen + 1);

    if (req->content_len > MAX_FILE_SIZE) {
        ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File size must be less than " MAX_FILE_SIZE_STR "!");
        return ESP_FAIL;
    }

    fd = fopen(filepath, "w");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to create file : %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Receiving file : %s...", filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char* buf = REQ_CTX->scratch;
    int received;

    /* Content length of the request gives
     * the size of the file being uploaded */
    int remaining = req->content_len;

    while (remaining > 0) {
        ESP_LOGI(TAG, "Remaining size : %d", remaining);
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }

            fclose(fd);
            unlink(filepath);

            ESP_LOGE(TAG, "File reception failed!");

            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        if (received && (received != fwrite(buf, 1, received, fd))) {
            fclose(fd);
            unlink(filepath);
            ESP_LOGE(TAG, "File write failed!");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            return ESP_FAIL;
        }

        remaining -= received;
    }

    /* Close file upon upload completion */
    fclose(fd);
    ESP_LOGI(TAG, "File reception complete");

    set_resp_CORS_headers(req, "POST");
    httpd_resp_sendstr(req, "File uploaded successfully");
    return ESP_OK;
}

static esp_err_t projects_get_handler(httpd_req_t* req)
{
    char filepath[FILE_PATH_MAX];
    FILE* fd = NULL;
    struct stat file_stat;
    const char* filename = "/projects.json";

    const size_t base_pathlen = strlen(REQ_CTX->base_path);
    size_t pathlen = strlen(filename);
    strcpy(filepath, REQ_CTX->base_path);
    strlcpy(filepath + base_pathlen, filename, pathlen + 1);

    set_resp_CORS_headers(req, "GET");

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
    set_content_type_from_file(req, filename);
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char* chunk = REQ_CTX->scratch;
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
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }

        /* Keep looping till the whole file is sent */
    } while (chunksize != 0);

    fclose(fd);
    ESP_LOGI(TAG, "File sending complete");
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t projects_options_handler(httpd_req_t* req)
{
    set_resp_CORS_headers(req, "POST,GET");
    httpd_resp_sendstr(req, "Picture pixels upload method");
    return ESP_OK;
}

// HTTP Error (404) Handler - Redirects all requests to the root page
esp_err_t http_404_error_handler(httpd_req_t* req, httpd_err_code_t err)
{
    // Set status
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // Redirect to the "/" root directory
    httpd_resp_set_hdr(req, "Location", "/");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to root");
    return ESP_OK;
}

/* Function to start the file server */
esp_err_t start_file_server(const char* base_path)
{
    static struct file_server_data* server_data = NULL;

    if (server_data) {
        ESP_LOGE(TAG, "File server already started");
        return ESP_ERR_INVALID_STATE;
    }

    /* Allocate memory for server data */
    server_data = calloc(1, sizeof(struct file_server_data));
    if (!server_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for server data");
        return ESP_ERR_NO_MEM;
    }
    strlcpy(server_data->base_path, base_path, sizeof(server_data->base_path));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 20;
    config.lru_purge_enable = true;

    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start file server!");
        return ESP_FAIL;
    }

    httpd_uri_t system_info_get_uri = { .uri = "/api/v1/system/info", .method = HTTP_GET, .handler = system_info_get_handler, .user_ctx = server_data };
    httpd_register_uri_handler(server, &system_info_get_uri);

    httpd_uri_t set_picture_post_uri = { .uri = "/api/v1/led/picture", .method = HTTP_POST, .handler = set_picture_post_handler, .user_ctx = server_data };
    httpd_register_uri_handler(server, &set_picture_post_uri);

    httpd_uri_t set_picture_options_uri = { .uri = "/api/v1/led/picture", .method = HTTP_OPTIONS, .handler = set_picture_options_handler, .user_ctx = server_data };
    httpd_register_uri_handler(server, &set_picture_options_uri);

    httpd_uri_t projects_post_uri = { .uri = "/api/v1/projects", .method = HTTP_POST, .handler = projects_post_handler, .user_ctx = server_data };
    httpd_register_uri_handler(server, &projects_post_uri);

    httpd_uri_t projects_get_uri = { .uri = "/api/v1/projects", .method = HTTP_GET, .handler = projects_get_handler, .user_ctx = server_data };
    httpd_register_uri_handler(server, &projects_get_uri);

    httpd_uri_t projects_options_uri = { .uri = "/api/v1/projects", .method = HTTP_OPTIONS, .handler = projects_options_handler, .user_ctx = server_data };
    httpd_register_uri_handler(server, &projects_options_uri);

    httpd_uri_t file_download = {
        .uri = "/*", // Match all URIs of type /path/to/file
        .method = HTTP_GET,
        .handler = download_get_handler,
        .user_ctx = server_data // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_download);

    httpd_uri_t file_upload = {
        .uri = "/upload/*", // Match all URIs of type /upload/path/to/file
        .method = HTTP_POST,
        .handler = upload_post_handler,
        .user_ctx = server_data // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_upload);

    httpd_uri_t file_delete = {
        .uri = "/delete/*", // Match all URIs of type /delete/path/to/file
        .method = HTTP_POST,
        .handler = delete_post_handler,
        .user_ctx = server_data // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_delete);

    // httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);

    return ESP_OK;
}

esp_err_t mount_storage(const char* base_path)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = { .base_path = base_path,
        .partition_label = NULL,
        .max_files = 5, // This sets the maximum number of files that can be open at the same time
        .format_if_mount_failed = true };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0;
    size_t used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    return ESP_OK;
}