#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"

/* Scratch buffer size */
#define SCRATCH_BUFSIZE 8192
#define WEB_TH          16    

struct file_server_data {
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];
    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};

typedef struct {
    uint8_t *pdata;
    uint32_t len;
}frame;

extern const char *TAG;
extern bool config;
extern QueueHandle_t frameq;
extern unsigned char wen;
extern SemaphoreHandle_t xSemaphore;
frame wf = {0};

static esp_err_t http_resp_config_html(httpd_req_t *req, const char *dirpath){
    char entrysize[16];
    struct dirent *entry;
    struct stat entry_stat;
    DIR *dir = opendir(dirpath);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to stat dir : %s", dirpath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
        return ESP_FAIL;
    }
    entry = readdir(dir);
    if(entry == NULL){
        ESP_LOGI(TAG, "Directory Empty : %s", dirpath);
    }else{
        if (stat("/flash/config.txt", &entry_stat) == -1) {
            ESP_LOGE(TAG, "Failed to stat for file");
        }
        sprintf(entrysize, "%ld", entry_stat.st_size); 
    }
    closedir(dir);
   
    /* Send HTML file header */
    httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><body><h2>ECSYS Configuration[");
    httpd_resp_sendstr_chunk(req, entrysize);    
    httpd_resp_sendstr_chunk(req, " Bytes]</h2><h4>Sample Configuration</h4>");
    httpd_resp_sendstr_chunk(req, "<p>ssid = sarath_nivas_EXT #wifi ssid<br>");
    httpd_resp_sendstr_chunk(req, "pass = land1234 #wifi password<br>");
    httpd_resp_sendstr_chunk(req, "jpeg = 100 #jpeg image quality 0 - 100<br>");
    httpd_resp_sendstr_chunk(req, "beacon = 15 #beacon time out 1-255 seconds<br>");
    httpd_resp_sendstr_chunk(req, "mouse = 3 #mouse level sensitivity L=0,L+R=1,L+R+M=2,L+R+M+S=3,L+R+M+S+movement=4<br>");
    httpd_resp_sendstr_chunk(req, "key = espkey #secret shared between wifi alarm and ecsys<br>");
    httpd_resp_sendstr_chunk(req, "mdini = 16,24 #start time  0-23, duration 0 -24 in hrs, 0 = disable<br>");
    httpd_resp_sendstr_chunk(req, "mdth = 1024 #motion detection threshold 1 - 2^32<br>");
    httpd_resp_sendstr_chunk(req, "mdroi = 40,40,560,400 #x,y,w,h on 640x480 ROI for motion detection<br>");
    httpd_resp_sendstr_chunk(req, "mdalrm = yes # if yes alarm will ring on motion detection<br>");
    httpd_resp_sendstr_chunk(req, "tz = UTC-5:30 #setting timezone for India<br>");
    httpd_resp_sendstr_chunk(req, "ts = 2025,02,20,10,30 #if NTP fails this year,month,date,hour,minute will be time at bootup<br>");
    httpd_resp_sendstr_chunk(req, "sdcard = yes #enable = yes [MAKE SURE SDCARD IS INSERTED FOR SYSTEM BOOT UP if enabled]<br>");
    httpd_resp_sendstr_chunk(req, "led = yes #enable = yes else disabled blinking led<br>");
    httpd_resp_sendstr_chunk(req, "msg = 1234567890 #user specific message</p>");
    httpd_resp_sendstr_chunk(req, "<form action=\"/config\" method=\"post\" enctype=\"text/plain\"><textarea id=\"edit\" name=\"edit\" rows=\"20\" cols=\"100\">");

    FILE *fin = fopen("/flash/config.txt", "r");
    if (fin == NULL){
        ESP_LOGE(TAG, "File read failed!");
    }else{
        char line[256];
        while (fgets(line, sizeof(line), fin) != NULL)httpd_resp_sendstr_chunk(req, line);
        fclose(fin);
    }    
    /* Send remaining chunk of HTML file to complete it */
    httpd_resp_sendstr_chunk(req, "</textarea> <br> <input type=\"submit\" value=\"Submit\"></form></body></html>");
    /* Send empty chunk to signal HTTP response completion */
    httpd_resp_sendstr_chunk(req, NULL);    
    return ESP_OK;
}

static esp_err_t http_resp_apsta_html(httpd_req_t *req, const char *dirpath){  
    wen = WEB_TH;
    frame f;
    if(xQueueReceive(frameq,&f,portMAX_DELAY) == pdTRUE){
        if(wf.pdata)heap_caps_free(wf.pdata);
        memcpy((void *)&wf,(void* )&f,sizeof(frame));
    }
    httpd_resp_set_hdr(req, "Refresh","1");
    httpd_resp_set_hdr(req, "Content-Disposition","inline");
    httpd_resp_set_hdr(req, "Content-type","image/jpeg");
    httpd_resp_send(req, (const char *)wf.pdata,wf.len);
    return ESP_OK;
}

/* Handler to download a file kept on the server */
static esp_err_t main_get_handler(httpd_req_t *req){
    if(config){
        return http_resp_config_html(req, "/flash/");
    }else{
        return http_resp_apsta_html(req, "/flash/");
    }
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Handler to upload a file onto the server */
static esp_err_t main_post_handler(httpd_req_t *req){
    ESP_LOGE(TAG, "POST");
    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
    int received = 0;
    /* Content length of the request gives
     * the size of the file being uploaded */
    int remaining = req->content_len;
    while (remaining > 0) {
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry if timeout occurred */
                continue;
            }
            ESP_LOGE(TAG, "File reception failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }
        remaining -= received;
    }
    /* Close file upon upload completion */
    FILE *fout = fopen("/flash/config.txt", "w");
    if (fout == NULL){
        ESP_LOGE(TAG, "Failed to write failed!");
        return ESP_FAIL;
    }
    char line[256];
    for(int i = 5,j =0;i < received;i++){
        char c = *(buf+i);
        if((c != '\n') && (c != '\r')){
            line[j] = c;
            j++;
        }else{
            line[j] = '\n';
            line[j+1] = '\0';
            if(strlen(line))fprintf(fout, line);
            i++;
            j = 0;
        }
    }
    fclose(fout);
    
    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Function to start the file server */
esp_err_t start_file_server(const char *base_path){
    static struct file_server_data *server_data = NULL;

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
    strlcpy(server_data->base_path, base_path,sizeof(server_data->base_path));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start file server!");
        return ESP_FAIL;
    }

    /* URI handler for getting uploaded files */
    httpd_uri_t main_get = {
        .uri       = "/*",  // Match all URIs of type /path/to/file
        .method    = HTTP_GET,
        .handler   = main_get_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &main_get);

    /* URI handler for uploading files to server */
    httpd_uri_t main_post = {
        .uri       = "/config",   // Match all URIs of type /upload/path/to/file
        .method    = HTTP_POST,
        .handler   = main_post_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &main_post);
    return ESP_OK;
}
