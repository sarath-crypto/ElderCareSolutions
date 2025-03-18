#include <stdbool.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#include "esp_vfs_fat.h"
#include "esp_attr.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_err.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "esp_camera.h"
#include "esp_jpeg_common.h"
#include "esp_jpeg_enc.h"
#include "esp_ldo_regulator.h"

#include "soc/gpio_struct.h"
#include "soc/uart_struct.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/sdmmc_host.h"

#include "sdkconfig.h"
#include "web.h"
#include "nvs_flash.h"

#include "sdmmc_cmd.h"
#include "errno.h"
#include "ssd1306.h"
#include "led_strip.h"


#include "usb/usb_host.h"
#include "usb/hid_host.h"
#include "usb/hid_usage_mouse.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/ip_addr.h"
#include <lwip/netdb.h>

#define portTICK_RATE_MS        portTICK_PERIOD_MS

#define SWITCH_GPIO             GPIO_NUM_1
#define LED_GPIO                GPIO_NUM_48

#define I2C0_MASTER_PORT        I2C_NUM_0
#define I2C0_MASTER_SDA_IO      GPIO_NUM_47 
#define I2C0_MASTER_SCL_IO      GPIO_NUM_21 
#define I2C0_HW_ADDR            0x3C
#define I2C_TH                  8

#define BASE_PATH_FLASH         "/flash"
#define BASE_PATH_SDCARD        "/sdcard"

#define STA_MAXIMUM_RETRY       2
#define TASK_PRIORITY           (tskIDLE_PRIORITY + 2)

#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT           BIT1

#define AP_SSID                 "ECSYS_AP"
#define AP_PASS                 "ECSYS_PASS"
#define AP_CHANNEL              1
#define AP_MAX_CONN             2

#define PORT                    8880
#define PDU_MTU	                1450
#define HEADER_LEN	            2
#define ALRM_DELAY              1200
#define BEACON_TH               6
#define PATTERN_SZ              10
#define NTP_TH                  8
#define CONFIG_TH               8

#define VIDEO_W                 640
#define VIDEO_H                 480

#define CAM_PIN_PWDN            38
#define CAM_PIN_RESET           -1  
#define CAM_PIN_VSYNC           6
#define CAM_PIN_HREF            7
#define CAM_PIN_PCLK            13
#define CAM_PIN_XCLK            15
#define CAM_PIN_SIOD            4
#define CAM_PIN_SIOC            5
#define CAM_PIN_D0              11
#define CAM_PIN_D1              9
#define CAM_PIN_D2              8
#define CAM_PIN_D3              10
#define CAM_PIN_D4              12
#define CAM_PIN_D5              18
#define CAM_PIN_D6              17
#define CAM_PIN_D7              16

//#define DEBUG                   1
#ifndef DEBUG
    #define LOG_LOCAL_LEVEL ESP_LOG_NONE
#endif

typedef struct{
    char fname[256];
    uint64_t ts;
}finfo;

typedef struct {
    uint8_t *pdata;
    uint32_t len;
}frame;

typedef struct {
        unsigned char r;
        unsigned char g;
        unsigned char b;
}PIXEL_RGB;

typedef struct {
	unsigned char	type;
	unsigned char   len;
	unsigned char 	data[PDU_MTU];
}pdu;

typedef struct {
    struct {
        hid_host_device_handle_t handle;
        hid_host_driver_event_t event;
        void *arg;
    } hid_host_device;
}app_event_queue_t;

typedef struct{
    bool mden;
    bool sden;
    bool sta;
    bool mdalrm;
    bool led_mode;
    bool led_rst;
    bool leden;
    uint8_t beacon;
    uint8_t sta_retry_num;
    uint8_t mouse;
    uint8_t alrm;
    uint8_t con;
    uint8_t jq;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t fc;
    uint16_t ticks;
    int32_t prev_x;
    int32_t prev_y;
    uint32_t mdth;
    EventGroupHandle_t wifi_event_group;
    QueueHandle_t appq;
    QueueHandle_t diskq;
    led_strip_handle_t led_strip;
    uint8_t *pRGB; 
    uint8_t *ppFm; 
    uint8_t *pcFm;   
    uint8_t *pdFm;    
    char ip[16];
    char key[32];
    uint16_t roi[4];
}ipc;    

enum pdu_type{KAL= 1,ALM};
enum dsp_state{CFG=1,NOC,CON,ALR,WEB};

const char *TAG = "ecsysapp";
bool config = false;
QueueHandle_t frameq;
uint8_t wen = 0;
ipc *pipc = NULL;


static void set_led(uint8_t red,uint8_t green,uint8_t blue,bool m){
    pipc->r = red;
    pipc->g = green;
    pipc->b = blue;
    pipc->led_mode = m;
    pipc->led_rst = true;
}

static void init_led(void){
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1, 
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &pipc->led_strip));
    led_strip_clear(pipc->led_strip);
    if(pipc->leden){
        set_led(0,0,255,false);
        led_strip_set_pixel(pipc->led_strip,0,pipc->r,pipc->g,pipc->b);
        led_strip_refresh(pipc->led_strip);
    }
}

static void on_timer(TimerHandle_t xTimer){
    pipc->ticks++;
    static uint8_t rc,gc,bc;
    static bool dir;
    if(pipc->led_rst){
        rc = 0;
        gc = 0;
        bc = 0;
        dir = true;
        pipc->led_rst = false;
    }
    if(pipc->leden){
        if(pipc->led_mode){
                if(dir){
                    if(rc < pipc->r)rc++;
                    if(gc < pipc->g)gc++;
                    if(bc < pipc->b)bc++;
                    if((rc >= pipc->r) && (gc >= pipc->g) && (bc >= pipc->b))dir = false;
                }else{
                    if(rc)rc--;
                    if(gc)gc--;
                    if(bc)bc--;
                    if(!rc && !gc && !bc)dir = true;
                }
        }else{
            rc = pipc->r;
            gc = pipc->g;
            bc = pipc->b;
        }
        led_strip_set_pixel(pipc->led_strip,0,rc,gc,bc);
        led_strip_refresh(pipc->led_strip);
    }
}

static void get_param(char *key){
    FILE *fin = fopen("/flash/config.txt", "r");
    if (fin == NULL){
        ESP_LOGE(TAG, "File read failed!");
    }else{
        char line[256];
        while (fgets(line, sizeof(line), fin) != NULL){
            int len = strlen(line);
            for(int i = 0,j = 0;i != len;i++){
                line[j] = line[i];
                if(line[i] != ' ')j++;
            }
            char param[64];
            for(int i = 0;i != len;i++){
                param[i] = line[i];
                if((line[i] == '=')){
                    param[i] = '\0';
                    break;
                } 
            }        
            if(strcmp(param,key) == 0){
                int i = 0;
                while(line[i] != '=')i++;
                i++;
                for(int j = 0;i != len;i++,j++){
                    if((line[i] == ' ') || (line[i] == '\n') || (line[i] == '\r') || (line[i] == '#')){
                        *(key + j ) = '\0';
                        break;
                    }else *(key + j) = line[i];
                }
                break;
            }
        }
        fclose(fin);
    } 
}

static void sta_event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data){
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (pipc->sta_retry_num < STA_MAXIMUM_RETRY) {
            esp_wifi_connect();
            pipc->sta_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(pipc->wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        sprintf(pipc->ip,"%d.%d.%d.%d",IP2STR(&event->ip_info.ip));
        pipc->sta_retry_num = 0;
        xEventGroupSetBits(pipc->wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(char *pssid,char *ppass){
    pipc->wifi_event_group = xEventGroupCreate();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&sta_event_handler,NULL,&instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP,&sta_event_handler,NULL,&instance_got_ip));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = {0},
            .password = {0},
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_HUNT_AND_PECK,
            .sae_h2e_identifier = "",
        },
    };
    strncpy((char *)wifi_config.sta.ssid,pssid,strlen(pssid));
    strncpy((char *)wifi_config.sta.password,ppass,strlen(ppass));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );
    EventBits_t bits = xEventGroupWaitBits(pipc->wifi_event_group,WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,pdFALSE,pdFALSE,portMAX_DELAY);
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:[%s] password:[%s]",pssid,ppass);
    } else if (bits & WIFI_FAIL_BIT) {
        pipc->sta = false;
        ESP_LOGI(TAG, "Failed to connect to SSID:[%s], password:[%s]",pssid,ppass);
    } else {
        pipc->sta  = false;
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

static void ap_event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data){
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}

static void wifi_init_softap(void){
    esp_netif_t* wifiAP = esp_netif_create_default_wifi_ap();
    esp_netif_ip_info_t ipInfo;
    IP4_ADDR(&ipInfo.ip, 10,10,10,1);
	IP4_ADDR(&ipInfo.gw, 10,10,10,1);
	IP4_ADDR(&ipInfo.netmask, 255,255,255,0);
	esp_netif_dhcps_stop(wifiAP);
	esp_netif_set_ip_info(wifiAP, &ipInfo);
	esp_netif_dhcps_start(wifiAP);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&ap_event_handler,NULL,NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .channel = AP_CHANNEL,
            .password = AP_PASS,
            .max_connection = AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                    .required = false,
            },
        },
    };
    if (strlen(AP_PASS) == 0)wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",AP_SSID, AP_PASS, AP_CHANNEL);
}

static void sort_files(finfo *pfi,uint8_t n){
        for(uint8_t j = 0;j < n;j++){
            for(uint8_t k = j+1;k < n;k++){
                if(pfi[j].ts < pfi[k].ts){
                    char fname[256];
                    strcpy(fname,pfi[j].fname);
                    strcpy(pfi[j].fname,pfi[k].fname);
                    strcpy(pfi[k].fname,fname);
                    uint64_t ts = pfi[j].ts;
                    pfi[j].ts = pfi[k].ts;
                    pfi[k].ts = ts;
                }
            }
        }
}

static esp_err_t init_camera(void){
    camera_config_t camera_config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,
    
        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,
    
        //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
        .xclk_freq_hz = 8000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
    
        .pixel_format = PIXFORMAT_RGB565, //YUV422,GRAYSCALE,RGB565,JPEG
        .frame_size = FRAMESIZE_VGA,    //QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. 
        .jpeg_quality = 0,              //0-63, for OV series camera sensors, lower number means higher quality
        .fb_count = 1,                  //When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    };
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK){
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }
    return ESP_OK;
}

static jpeg_error_t esp_jpeg_encode(uint8_t *pdata,uint32_t *plen,uint8_t q){
    // configure encoder
    jpeg_enc_config_t jpeg_enc_cfg = DEFAULT_JPEG_ENC_CONFIG();
    jpeg_enc_cfg.width = VIDEO_W;
    jpeg_enc_cfg.height = VIDEO_H;
    jpeg_enc_cfg.src_type = JPEG_PIXEL_FORMAT_RGB888;
    //jpeg_enc_cfg.src_type = JPEG_PIXEL_FORMAT_GRAY;
    jpeg_enc_cfg.subsampling = JPEG_SUBSAMPLE_420;
    //jpeg_enc_cfg.subsampling = JPEG_SUBSAMPLE_GRAY;
    jpeg_enc_cfg.quality = q;
    jpeg_enc_cfg.rotate = JPEG_ROTATE_0D;
    jpeg_enc_cfg.task_enable = false;
    jpeg_enc_cfg.hfm_task_priority = 13;
    jpeg_enc_cfg.hfm_task_core = 1;

    jpeg_error_t ret = JPEG_ERR_OK;
    jpeg_enc_handle_t jpeg_enc = NULL;
    ret = jpeg_enc_open(&jpeg_enc_cfg, &jpeg_enc);
    if (ret != JPEG_ERR_OK)return ret;
    int len = 0;
   
    ret = jpeg_enc_process(jpeg_enc, pdata, jpeg_enc_cfg.width * jpeg_enc_cfg.height * 3, pdata, jpeg_enc_cfg.width * jpeg_enc_cfg.height * 3, &len);
    *plen = (uint32_t)len;
    jpeg_enc_close(jpeg_enc);
    return ret;
}

static esp_err_t mount_storage_flash(const char* base_path){
    ESP_LOGI(TAG, "Initializing SPIFFS");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = base_path,
        .partition_label = NULL,
        .max_files = 1,   // This sets the maximum number of files that can be open at the same time
        .format_if_mount_failed = true
    };

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

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGW(TAG, "Flash Partition size: total: %d, used: %d", total, used);
    return ESP_OK;
}

static bool md_init(void){
    if (pipc->pRGB) {
        heap_caps_free(pipc->pRGB);
        pipc->pRGB = NULL;
    }
    pipc->pRGB= (uint8_t*)heap_caps_malloc(VIDEO_W * VIDEO_H  * 3, MALLOC_CAP_8BIT);
    if (!pipc->pRGB) return false;
    memset(pipc->pRGB, 0, VIDEO_W * VIDEO_H * 3); 

    if (pipc->ppFm) {
        heap_caps_free(pipc->ppFm);
        pipc->ppFm = NULL;
    }
    pipc->ppFm = (uint8_t*)heap_caps_malloc(VIDEO_W * VIDEO_H, MALLOC_CAP_8BIT);
    if (!pipc->ppFm) return false;
    memset(pipc->ppFm, 0, VIDEO_W * VIDEO_H); 

    if (pipc->pcFm) {
        heap_caps_free(pipc->pcFm);
        pipc->pcFm = NULL;
    }
    pipc->pcFm = (uint8_t*)heap_caps_malloc(VIDEO_W * VIDEO_H , MALLOC_CAP_8BIT);
    if (!pipc->pcFm) return false;
    memset(pipc->pcFm, 0, VIDEO_W * VIDEO_H); 

    if (pipc->pdFm) {
        heap_caps_free(pipc->pdFm);
        pipc->pdFm = NULL;
    }
    pipc->pdFm = (uint8_t*)heap_caps_malloc(VIDEO_W * VIDEO_H , MALLOC_CAP_8BIT);
    if (!pipc->pdFm) return false;
    memset(pipc->pdFm, 0, VIDEO_W * VIDEO_H); 

    return true;
}

static void md_blur(uint8_t *img){
	uint8_t k[3][3];
	memset(k,0,sizeof(k));
	for(uint16_t x = 0,y = 0; y < VIDEO_H-1;){
		bool a = true,b = true,c = true,d = true;
		if(x == pipc->roi[0]){
			k[0][0] = 0;
			k[1][0] = 0;
			k[2][0] = 0;
			a = false;
		}
		if(x == (pipc->roi[0]+pipc->roi[2])){
			k[0][2] = 0;
			k[1][2] = 0;
			k[2][2] = 0;
			b = false;
		}		
		if(y == pipc->roi[1]){
			k[0][0] = 0;
			k[0][1] = 0;
			k[0][2] = 0;
			c = false;
		}
		if(y == (pipc->roi[1]+pipc->roi[3])){
			k[2][0] = 0;
			k[2][1] = 0;
			k[2][2] = 0;
			d = false;
		}        
        if((x >= pipc->roi[0]) && (x <= (pipc->roi[0]+pipc->roi[2])) && (y >= pipc->roi[1]) && (y <= (pipc->roi[1]+pipc->roi[3]))){
            if(a && c)k[0][0] = img[((y-1)*VIDEO_W)-(VIDEO_W-(x-1))];
            if(c)k[0][1] =      img[((y-1)*VIDEO_W)-(VIDEO_W-x)];
            if(b && c)k[0][2] = img[((y-1)*VIDEO_W)-(VIDEO_W-(x+1))];
    
            if(a)k[1][0] = img[(y*VIDEO_W)-(VIDEO_W-(x-1))];
            k[1][1] =      img[(y*VIDEO_W)-(VIDEO_W-x)];
            if(b)k[1][2] = img[(y*VIDEO_W)-(VIDEO_W-(x+1))];
            
            if(a && d)k[2][0] = img[((y+1)*VIDEO_W)-(VIDEO_W-(x-1))];
            if(d)k[2][1] =      img[((y+1)*VIDEO_W)-(VIDEO_W-x)];
            if(b && d)k[2][2] = img[((y+1)*VIDEO_W)-(VIDEO_W-(x+1))];    
            img[(y*VIDEO_W)-(VIDEO_W-x)] = (k[0][0]+k[0][1]+k[0][2]+k[1][0]+k[1][2]+k[2][0]+k[2][1]+k[2][2])/8;
        }
        if(x < VIDEO_W-1)x++;
        else if( y < VIDEO_H-1){
            x = 0;
            y++;
        }
	}
}

static void md_thrh(uint8_t *img,uint8_t thl,uint8_t thh){
    for(uint16_t x = 0,y = 0; y < VIDEO_H-1;){
        if((x >= pipc->roi[0]) && (x <= (pipc->roi[0]+pipc->roi[2])) && (y >= pipc->roi[1]) && (y <= (pipc->roi[1]+pipc->roi[3]))){  
            uint8_t g = img[(y*VIDEO_W)-(VIDEO_W-x)];
            if(g < thl)g = 0;
            else if(g > thh )g = 255;
            img[(y*VIDEO_W)-(VIDEO_W-x)] = g;
        }
        if(x < VIDEO_W-1)x++;
        else if( y < VIDEO_H-1){
            x = 0;
            y++;
        }
    }
}

static void time_sync_notification_cb(struct timeval *tv){
    settimeofday(tv, NULL);
}

static bool get_ntp(uint16_t y,uint8_t m,uint8_t d,uint8_t h,uint8_t n,char *ptz){
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");    
    config.sync_cb = time_sync_notification_cb;     
    esp_netif_sntp_init(&config);
    uint8_t retry = 0;
    bool ret = false;
    set_led(0,0,255,true);
    while((esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT) && (retry < NTP_TH)){
        ESP_LOGI(TAG,"NTP Sync %d",retry);
        retry++;
    }
    set_led(0,0,255,false);
    if(retry >= NTP_TH){
        ret = false;
        struct tm timeinfo = { 0 };    
        timeinfo.tm_sec = 0;
        timeinfo.tm_min = n; 
        timeinfo.tm_hour = h;   
        timeinfo.tm_year = y-1900;
        timeinfo.tm_mon = m-1;
        timeinfo.tm_mday = d;
        time_t t = mktime(&timeinfo);
        struct timeval now = { .tv_sec = t };
        settimeofday(&now,NULL);
    }else{
        ret = true;
        setenv("TZ",ptz,1);
        tzset();
    }
    esp_netif_sntp_deinit();
    return ret;
}

static void hid_host_mouse_processor(const uint8_t *const data, const int length){
    hid_mouse_input_report_boot_t *mouse_report = (hid_mouse_input_report_boot_t *)data;
    if (length < sizeof(hid_mouse_input_report_boot_t)) return;
    static int x_pos = 0;
    static int y_pos = 0;
 
    x_pos += mouse_report->x_displacement;
    y_pos += mouse_report->y_displacement;
    unsigned char scrollup[] = {0x00,0x00,0x00,0x01};
    unsigned char scrolldn[] = {0x00,0x00,0x00,0xff};
 
    if(!pipc->alrm){
        switch(pipc->mouse){
            case(4):if((pipc->prev_x != x_pos) || (pipc->prev_x != y_pos))pipc->alrm = 1;
            case(3):if(!memcmp(data,scrollup,length) || !memcmp(data,scrolldn,length))pipc->alrm = 1;
            case(2):if(mouse_report->buttons.button3)pipc->alrm = 1;
            case(1):if(mouse_report->buttons.button2)pipc->alrm = 1;
            case(0):if(mouse_report->buttons.button1)pipc->alrm = 1;
        }
    }
    pipc->prev_x = x_pos;
    pipc->prev_y = y_pos;
    fflush(stdout);
    ESP_LOGW(TAG,"hid_host_mouse_proccesor %d %d %d",length,pipc->mouse,pipc->alrm);
}

static void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,const hid_host_interface_event_t event,void *arg){
    uint8_t data[64] = { 0 };
    size_t data_length = 0;
    static const char *hid_proto_name_str[] = {
        "NONE",
        "KEYBOARD",
        "MOUSE"
    };

    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(hid_device_handle,data,64,&data_length));
        if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class){
            if (HID_PROTOCOL_MOUSE == dev_params.proto)hid_host_mouse_processor(data, data_length);
        }
        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HID Device, protocol '%s' DISCONNECTED",hid_proto_name_str[dev_params.proto]);
        ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        ESP_LOGI(TAG, "HID Device, protocol '%s' TRANSFER_ERROR",hid_proto_name_str[dev_params.proto]);
        break;
    default:
        ESP_LOGE(TAG, "HID Device, protocol '%s' Unhandled event",hid_proto_name_str[dev_params.proto]);
        break;
    }
}

static void hid_host_device_event(hid_host_device_handle_t hid_device_handle,const hid_host_driver_event_t event,void *arg){
    hid_host_dev_params_t dev_params;
    static const char *hid_proto_name_str[] = {
        "NONE",
        "KEYBOARD",
        "MOUSE"
    };
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));
    switch (event) {
    case HID_HOST_DRIVER_EVENT_CONNECTED:
        ESP_LOGI(TAG, "HID Device, protocol '%s' CONNECTED",hid_proto_name_str[dev_params.proto]);
        const hid_host_device_config_t dev_config = {
            .callback = hid_host_interface_callback,
            .callback_arg = NULL
        };

        ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
        if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
            ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_BOOT));
            if (HID_PROTOCOL_KEYBOARD == dev_params.proto)ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
        }
        ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
        break;
    default:
        break;
    }
}

static void hid_host_device_callback(hid_host_device_handle_t hid_device_handle,const hid_host_driver_event_t event,void *arg){
    const app_event_queue_t evt_queue = {
        .hid_host_device.handle = hid_device_handle,
        .hid_host_device.event = event,
        .hid_host_device.arg = arg
    };
    xQueueSend(pipc->appq, &evt_queue, 0);
}

static void usb_lib_task(void *arg){
    ESP_LOGW(TAG, "init usb_lib_task");
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };

    ESP_ERROR_CHECK(usb_host_install(&host_config));
    xTaskNotifyGive(arg);

    while (true) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
            break;
        }
#ifdef DEBUG
        ESP_LOGW(TAG, "loop usb_lib_task");
#endif
        vTaskDelay(10/portTICK_PERIOD_MS); 
    }

    ESP_LOGI(TAG, "USB shutdown");
    // Clean up USB Host
    vTaskDelay(10); // Short delay to allow clients clean-up
    ESP_ERROR_CHECK(usb_host_uninstall());
    vTaskDelete(NULL);
}

static void hid_task(void *pvParameters){
    ESP_LOGW(TAG, "init hid_task");
    BaseType_t task_created;
    app_event_queue_t evt_queue;
    task_created = xTaskCreatePinnedToCore(usb_lib_task,"usb_events",4096,xTaskGetCurrentTaskHandle(),2, NULL, 0);
    assert(task_created == pdTRUE);
    ulTaskNotifyTake(false, 1000);
    const hid_host_driver_config_t hid_host_driver_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_host_device_callback,
        .callback_arg = NULL
    };
    ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));
    pipc->appq = xQueueCreate(2, sizeof(app_event_queue_t));
    while (true) {
        if (xQueueReceive(pipc->appq, &evt_queue, portMAX_DELAY)) {
            hid_host_device_event(evt_queue.hid_host_device.handle,evt_queue.hid_host_device.event,evt_queue.hid_host_device.arg);
        }
#ifdef DEBUG
        ESP_LOGW(TAG, "loop hid_task");
#endif
        vTaskDelay(10/portTICK_PERIOD_MS); 
    }
    ESP_LOGI(TAG, "HID Driver uninstall");
    ESP_ERROR_CHECK(hid_host_uninstall());
    xQueueReset(pipc->appq);
    vQueueDelete(pipc->appq);
    vTaskDelete(NULL);
}

static void udp_server_task(void *ptr){
    ESP_LOGW(TAG, "init udp_server_task");
    char rx_buffer[32];
    char addr_str[32];
    struct sockaddr_in dest_addr_ip4;
    dest_addr_ip4.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4.sin_family = AF_INET;
    dest_addr_ip4.sin_port = htons(PORT);
   
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    struct sockaddr_storage source_addr; 
    socklen_t socklen = sizeof(source_addr);

    while (true) {
        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof timeout);
        int err = bind(sock,(struct sockaddr *)&dest_addr_ip4,sizeof(dest_addr_ip4));
        if (err < 0)ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);

        unsigned char sec = 0;
        pdu p;
        while (sec < pipc->beacon) {
            if(pipc->alrm == 1)break;
            sec++;
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
            if (len > 0){
                if (source_addr.ss_family == PF_INET)inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
                memcpy((void *)&p,rx_buffer,len);
                char rxkey[32];
                strncpy(rxkey,(char *)p.data,p.len-HEADER_LEN);
                ESP_LOGW(TAG, "Received %d bytes from %s Type %d Con %d rxKey %s Key %s ", len, addr_str,p.type,pipc->con,rxkey,pipc->key);
                if((p.type == KAL) && !strcmp(rxkey,pipc->key))pipc->con = 1;
            }  
        }
        if(pipc->con){
            if(pipc->alrm <= 1){
                if(pipc->alrm == 1){
                    pipc->alrm = 2;
                    p.type = ALM;
                }
                else p.type = KAL;
                
                p.len = HEADER_LEN+strlen(pipc->key);
                memcpy((void *)&p.data,pipc->key,strlen(pipc->key));
                ESP_LOGW(TAG, "Send %d bytes to %s Type %d Alrm %d Con %d", p.len, addr_str,p.type,pipc->alrm,pipc->con);
                err = sendto(sock,(char *)&p,p.len, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                if (err < 0)ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                pipc->con++;
            }
            if(pipc->con >= BEACON_TH)pipc->con = 0;;
        }
        if (sock != -1) {
            shutdown(sock, 0);
            close(sock);
        }
#ifdef DEBUG
        ESP_LOGW(TAG, "loop udp_server_task");
#endif
        vTaskDelay(200/portTICK_PERIOD_MS); 
    }
    vTaskDelete(NULL);
}

static void md_cam_task(void *arg){
    ESP_LOGW(TAG, "init md_cam_task");
    if(ESP_OK != init_camera()){
        ESP_LOGE(TAG, "Camera Failed");
        return;
    }
    if(!md_init()){
        ESP_LOGE(TAG, "motion detect failed");
        return;
    }
    frame fq;
    camera_fb_t *fb = NULL;
    while (true){
        fb = esp_camera_fb_get();
        if(!fb)continue;
        if(fb->len != (VIDEO_H*VIDEO_W*2)){
            esp_camera_fb_return(fb); 
            continue;
        }
        memset(pipc->pcFm,0,VIDEO_W*VIDEO_H);
        memset(pipc->pdFm,0,VIDEO_W*VIDEO_H);
        for(uint32_t i = 0,j = 0,x = 0,y = 0,k = 0; i < VIDEO_W * VIDEO_H * 2;i+=2,j+=3,k++){
            //RGB565 -> RGB888
            uint16_t pix = fb->buf[i] << 8 | fb->buf[i+1];
            pipc->pRGB[j+0] = ((pix >> 11) * 527 + 23 ) >> 6;
            pipc->pRGB[j+1] = (((pix >> 5 ) & (0x3F)) * 259 + 23 ) >> 6;
            pipc->pRGB[j+2] = ((pix &  0x1F) * 527 + 23 ) >> 6;
            if(pipc->mden){
                if((x >= pipc->roi[0]) && (x <= (pipc->roi[0]+pipc->roi[2])) && (y >= pipc->roi[1]) && (y <= (pipc->roi[1]+pipc->roi[3]))){
                    //RGB888 -> Grey
                    pipc->pcFm[k] = pipc->pRGB[j+0] * 0.2126 + pipc->pRGB[j+1] * 0.7152 + pipc->pRGB[j+2] * 0.0722;
                    pipc->pdFm[k] = pipc->pcFm[k]-pipc->ppFm[k];
                    pipc->ppFm[k] = pipc->pcFm[k];
                }
            }
            if((((x == pipc->roi[0]) || (x == (pipc->roi[0]+pipc->roi[2]))) && (y >= pipc->roi[1]) && (y <= (pipc->roi[1]+pipc->roi[3]))) ||
              (((y == pipc->roi[1]) || (y == (pipc->roi[1]+pipc->roi[3]))) && (x >= pipc->roi[0]) && (x <= (pipc->roi[0]+pipc->roi[2])))){
                pipc->pRGB[j+0]  = 0xff;
                pipc->pRGB[j+1]  = 0x55;
                pipc->pRGB[j+2]  = 0;
            }
            if(x < VIDEO_W-1)x++;
            else if( y < VIDEO_H-1){
                x = 0;
                y++;
            }
        }
        esp_camera_fb_return(fb); 
        fb = NULL;    
        if(pipc->mden){
            md_blur(pipc->pdFm);
            md_thrh(pipc->pdFm,4,5);
            md_blur(pipc->pdFm);
            md_thrh(pipc->pdFm,4,5);
            uint32_t pix  = 0;
            for(uint16_t x = 0,y = 0; y < VIDEO_H-1;){
                if((x >= pipc->roi[0]) && (x <= (pipc->roi[0]+pipc->roi[2])) && (y >= pipc->roi[1]) && (y <= (pipc->roi[1]+pipc->roi[3]))){
                    if(pipc->pdFm[(y*VIDEO_W)-(VIDEO_W-x)] < 255)pix++;  
                }
                if(x < VIDEO_W-1)x++;
                else if( y < VIDEO_H-1){
                    x = 0;
                    y++;
                }
            }
            if(pix >= pipc->mdth){
                if(pipc->mdalrm && !pipc->alrm)pipc->alrm = 1;
            }   
        }
        
        esp_jpeg_encode(pipc->pRGB,&fq.len,pipc->jq);
        fq.pdata = (uint8_t *)heap_caps_malloc(fq.len,MALLOC_CAP_8BIT);
        if (!fq.pdata)continue;
        memset(fq.pdata, 0, fq.len); 
        memcpy(fq.pdata,pipc->pRGB,fq.len);
        if(xQueueGenericSend(frameq,(void *)&fq,0,queueSEND_TO_BACK) != pdTRUE)heap_caps_free(fq.pdata);
        
        if(pipc->sden){
            frame dq;
            dq.len = fq.len;
            dq.pdata = (uint8_t *)heap_caps_malloc(dq.len,MALLOC_CAP_8BIT);
            if (!dq.pdata)continue;
            memset(dq.pdata, 0, dq.len); 
            memcpy(dq.pdata,pipc->pRGB,dq.len);
            if(xQueueGenericSend(pipc->diskq,(void *)&dq,0,queueSEND_TO_BACK) != pdTRUE)heap_caps_free(dq.pdata);
        }
#ifdef DEBUG
        ESP_LOGW(TAG, "loop md_cam_task");
#endif
        vTaskDelay(200/portTICK_PERIOD_MS); 
    }
    vTaskDelete(NULL);
}

static void disk_task(void *arg){
    ESP_LOGW(TAG, "init disk_task");
    esp_err_t ret;
    sdmmc_card_t *card = NULL;
    DIR *dir = NULL;
    
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 1,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_host_t sdhost = SDMMC_HOST_DEFAULT();
    sdhost.max_freq_khz = SDMMC_FREQ_PROBING;
    sdhost.flags |= SDMMC_HOST_FLAG_1BIT;
    
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;
    slot_config.clk = GPIO_NUM_39;
    slot_config.cmd = GPIO_NUM_38;
    slot_config.d0 = GPIO_NUM_40;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    
    ESP_LOGW(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdmmc_mount(BASE_PATH_SDCARD, &sdhost, &slot_config, &mount_config, &card);    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize the card (%s) formating...", esp_err_to_name(ret));
        ret = esp_vfs_fat_sdcard_format(BASE_PATH_SDCARD, card);
        if (ret != ESP_OK){
            ESP_LOGE(TAG, "Failed to format FATFS (%s)", esp_err_to_name(ret));
            pipc->sden = false;
            esp_vfs_fat_sdcard_unmount(BASE_PATH_SDCARD,card);
            sdmmc_host_deinit();
        }
    }
    if(pipc->sden){
        int freq_khz = 0;
        sdmmc_host_get_real_freq(SDMMC_HOST_SLOT_0, &freq_khz);
        sdmmc_host_state_t sdhost_state;
        sdmmc_host_get_state(&sdhost_state);
        ESP_LOGW(TAG, "Filesystem enabled freq[%d] %d",freq_khz,sdhost_state.host_initialized);

        dir = opendir(BASE_PATH_SDCARD);
        if (!dir) {
            ESP_LOGE(TAG, "Failed to stat dir : %s",BASE_PATH_SDCARD);
            pipc->sden = false;
        }else closedir(dir);
    }    
    while (true){
        frame f;
        if((xQueueReceive(pipc->diskq,&f,portMAX_DELAY) == pdTRUE) && pipc->sden){  
            time_t now;
            struct tm timeinfo;
            time(&now);    
            localtime_r(&now, &timeinfo);
            char ts[32];
            strftime(ts, sizeof(ts), "%d%H%M", &timeinfo);
            char fcs[3];
            itoa(pipc->fc,fcs,10);
            if(strlen(fcs) == 1)strcat(ts,"0");
            strcat(ts,fcs);
            strcat(ts,".jpg");

            if(pipc->fc < 99)pipc->fc++;
            else pipc->fc = 0;        
            char fn[32];
            strcpy(fn,BASE_PATH_SDCARD);
            strcat(fn,"/");
            strcat(fn,ts);

            FATFS *fs;
            DWORD fre_clust, fre_sect, tot_sect;
            int res = f_getfree(BASE_PATH_SDCARD, &fre_clust, &fs);
            if (!res) {
                tot_sect = (fs->n_fatent - 2) * fs->csize;
                fre_sect = fre_clust * fs->csize;
                uint64_t total_bytes = tot_sect * FF_SS_SDCARD;
                uint64_t free_bytes = fre_sect * FF_SS_SDCARD;        
               
                if(free_bytes > f.len){
                    FILE *fo = fopen(fn, "wb");
                    if (fo == NULL){
                        ESP_LOGE(TAG, "SDCard : Failed to open file for writing");
                        pipc->sden = false;
                    }else{
                        fwrite(f.pdata,1,f.len,fo);
                        fclose(fo);
                        ESP_LOGW(TAG,"%s[%lu] [%llu %llu]",fn,f.len,total_bytes,free_bytes);
                    }        
                }else{
                    ESP_LOGE(TAG, "SDCard is full deleting the older files");
                    dir = opendir(BASE_PATH_SDCARD);
                    struct dirent *entry = readdir(dir);
                    finfo fi[8] = {0};
                    uint8_t i = 0,n = 0;
                    while((entry = readdir(dir)) != NULL){
                        struct stat entry_stat;
                        strcpy(fn,BASE_PATH_SDCARD);
                        strcat(fn,"/");
                        strcat(fn,entry->d_name);        
                        if (stat(fn, &entry_stat) == -1){
                            ESP_LOGE(TAG, "Failed to stat of %s", entry->d_name);
                            continue;
                        }
                        strcpy(fi[i].fname,fn);
                        fi[i].ts = entry_stat.st_mtime;
                        if(i < 8){
                            i++;
                            if(n < 8)n = i;
                        }else{
                            i = 0;
                            n = 8;
                        }
                        sort_files(&fi[0],n);
                    }
                    closedir(dir);
                    for(uint8_t j = 0;j < n;j++){
                        ESP_LOGE(TAG, "Deleting file[%d]:%llu",j,fi[j].ts);
                        unlink(fi[j].fname);
                    }
                }
            }
            heap_caps_free(f.pdata);
        }
#ifdef DEBUG
        ESP_LOGW(TAG, "loop disk_task");
#endif
        vTaskDelay(10/portTICK_PERIOD_MS); 
    }
    vTaskDelete(NULL);
}

void app_main(void){
    char param_a[64];
    char param_b[64];
    uint8_t dsp_state = 0;
    time_t now = {0};
    struct tm timeinfo = {0};
    char save[4] = {'-','/','|','\\'};
    uint8_t ps = 0;
    bool ntpen = false;
    char msg[64] ={0};
    
    pipc = (ipc *)heap_caps_malloc(sizeof(ipc),MALLOC_CAP_8BIT);
    memset(pipc,0,sizeof(ipc)); 
    pipc->leden = true;
    pipc->sta = true;
    uint8_t prev_fc = pipc->fc;
    init_led();

    esp_log_level_set("ecsysapp",ESP_LOG_INFO);   
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(mount_storage_flash(BASE_PATH_FLASH));
    DIR *dir = opendir("/flash");
    if(!dir){
        ESP_LOGE(TAG, "Failed to stat dir : %s",BASE_PATH_FLASH);
        esp_restart();
    }
    struct dirent *entry= readdir(dir);
    if(!entry){
        ESP_LOGI(TAG, "Directory Empty : %s",BASE_PATH_FLASH);
        FILE *fout = fopen("/flash/config.txt", "w");
        if (fout == NULL){
            ESP_LOGE(TAG, "Failed to write basic coniguration erasing the flash!");
            ESP_ERROR_CHECK(nvs_flash_erase());
            esp_restart();
        }
        fprintf(fout, "ssid = myssid #wifi ssid\n");
        fprintf(fout, "pass = mypass #wifi password\n");
        fclose(fout);
        config = true;
    }
    closedir(dir);  

    strcpy(param_a,"ssid");
    get_param(param_a);
    strcpy(param_b,"pass");
    get_param(param_b);
    if(pipc->sta){
        ESP_LOGW(TAG, "ESP_WIFI_MODE_STA");
        wifi_init_sta(param_a,param_b);  
    }
    if(!pipc->sta){
        ESP_LOGW(TAG, "ESP_WIFI_MODE_AP");
        wifi_init_softap();
    }

    ESP_ERROR_CHECK(start_file_server(BASE_PATH_FLASH));
    TimerHandle_t xTimer = xTimerCreate("timer", pdMS_TO_TICKS(10), true, NULL, on_timer);
    xTimerStart(xTimer,0);

    strcpy(param_a,"led");
    get_param(param_a);

    if(!strcmp(param_a,"yes"))pipc->leden = true;
    else{
        set_led(0,0,0,false);
        vTaskDelay(100/portTICK_PERIOD_MS);
        pipc->leden = false;
    }

    gpio_reset_pin(SWITCH_GPIO);
    gpio_set_direction(SWITCH_GPIO, GPIO_MODE_INPUT);
    set_led(0,0,255,true);
    for(int i= 0;i < CONFIG_TH;i++){
        if(gpio_get_level(SWITCH_GPIO) == 0){
            config = true;
            dsp_state = CFG;
            break;
        }
        ESP_LOGW(TAG, "Waiting for Config user input %d",i);
        vTaskDelay(500/portTICK_PERIOD_MS);
    }
    set_led(0,0,255,false);
    i2c_ssd1306_config_t dev_cfg = I2C_SSD1306_128x32_CONFIG_DEFAULT;
    i2c_ssd1306_handle_t dev_hdl;
    i2c_master_bus_handle_t  i2c0_bus_hdl;
    i2c_master_bus_config_t  i2c0_bus_cfg =  {                                
        .clk_source                     = I2C_CLK_SRC_DEFAULT,      
        .i2c_port                       = I2C_NUM_0,         
        .scl_io_num                     = GPIO_NUM_21,       
        .sda_io_num                     = GPIO_NUM_47,       
        .glitch_ignore_cnt              = 7,                            
        .flags.enable_internal_pullup   = true, };
    
    ESP_ERROR_CHECK( i2c_new_master_bus(&i2c0_bus_cfg, &i2c0_bus_hdl) );
    set_led(0,0,255,true);
    for(uint8_t try= 0;try < I2C_TH;try++){
        ESP_LOGW(TAG, "i2C SSD1306 init retry %d",try);
        esp_err_t ret = i2c_ssd1306_init(i2c0_bus_hdl, &dev_cfg, &dev_hdl);
        if(ret == ESP_OK){
            if (dev_hdl == NULL) {
                ESP_LOGE(TAG, "ssd1306 handle init failed");
                assert(dev_hdl);
            }
            break;
        }
    }
 
    unsigned char durmap[24] = {0};
    if(!config){
        strcpy(param_a,"beacon");
        get_param(param_a);
        pipc->beacon = atoi(param_a);
        strcpy(param_a,"mouse");
        get_param(param_a);
        pipc->mouse = atoi(param_a);
        strcpy(param_a,"key");
        get_param(param_a);
        strcpy(pipc->key,param_a);

        unsigned char mdini = 0;
        unsigned char mddur = 0;

        strcpy(param_a,"mdini");
        get_param(param_a);
        int p = 0;
        int j = 0;
        while(param_a[p] != ','){
            param_b[j] = param_a[p];
            p++;
            j++;
        }
        param_b[j] = '\0';
        mdini = atoi(param_b);
        if(mdini > 23)mdini = 23;

        p++;
        j = 0;
        while(param_a[p] != '\0'){
            param_b[j] = param_a[p];
            p++;
            j++;
        }
        param_b[j] = '\0';
        mddur = atoi(param_b);     
        if(mddur > 24)mddur = 24;
        if(mddur){
            strcpy(param_a,"mdth");
            get_param(param_a);
            pipc->mdth = strtoul(param_a,NULL,0);
            strcpy(param_a,"mdroi");
            get_param(param_a);

            p = 0;
            j = 0;
            while(param_a[p] != ','){
                param_b[j] = param_a[p];
                p++;
                j++;
            }
            param_b[j] = '\0';
            pipc->roi[0] = atoi(param_b);
        
            p++;
            j = 0;
            while(param_a[p] != ','){
                param_b[j] = param_a[p];
                p++;
                j++;
            }
            param_b[j] = '\0';
            pipc->roi[1] = atoi(param_b);

            p++;
            j = 0;
            while(param_a[p] != ','){
                param_b[j] = param_a[p];
                p++;
                j++;
            }
            param_b[j] = '\0';
            pipc->roi[2] = atoi(param_b);

            p++;
            j = 0;
            while(param_a[p] != '\0'){
                param_b[j] = param_a[p];
                p++;
                j++;
            }
            param_b[j] = '\0';
            pipc->roi[3] = atoi(param_b);     

            strcpy(param_a,"mdalrm");
            get_param(param_a);
            if(!strcmp(param_a,"yes"))pipc->mdalrm = true;
        }
	    for(unsigned char h = mdini,i = 0;i < mddur;i++){
            durmap[h] = 1;
            if(h < 23)h++;
            else h = 0;
 	    }
        strcpy(param_a,"jpeg");
        get_param(param_a);
        pipc->jq = atoi(param_a);

        strcpy(param_a,"sdcard");
        get_param(param_a);
        if(!strcmp(param_a,"yes"))pipc->sden = true;

        frameq = xQueueCreate(2,sizeof(frame));
        if(frameq == NULL)ESP_LOGE(TAG,"Error on FrameQ");
        if(pipc->sden){
            pipc->diskq = xQueueCreate(2,sizeof(frame));
            if(pipc->diskq == NULL)ESP_LOGE(TAG,"Error on DiskQ");
            xTaskCreate(disk_task, "sd_card", 6144, NULL, 0, NULL);
            while(!pipc->sden);
        }
        xTaskCreate(md_cam_task, "md_cam", 4096, NULL,0, NULL);
        xTaskCreate(hid_task, "hid", 4096, NULL, TASK_PRIORITY, NULL); 
        xTaskCreate(udp_server_task, "udp_server",4096, NULL,TASK_PRIORITY, NULL);
    
        strcpy(param_a,"ts");
        get_param(param_a);
        p = 0;
        j = 0;
        while(param_a[p] != ','){
            param_b[j] = param_a[p];
            p++;
            j++;
        }
        param_b[j] = '\0';
        uint16_t y = atoi(param_b);

        p++;
        j = 0;
        while(param_a[p] != ','){
            param_b[j] = param_a[p];
            p++;
            j++;
        }
        param_b[j] = '\0';
        uint8_t m = atoi(param_b);

        p++;
        j = 0;
        while(param_a[p] != ','){
            param_b[j] = param_a[p];
            p++;
            j++;
        }
        param_b[j] = '\0';
        uint8_t d = atoi(param_b);

        p++;
        j = 0;
        while(param_a[p] != ','){
            param_b[j] = param_a[p];
            p++;
            j++;
        }
        param_b[j] = '\0';
        uint8_t h = atoi(param_b);

        p++;
        j = 0;
        while(param_a[p] != '\0'){
            param_b[j] = param_a[p];
            p++;
            j++;
        }
        param_b[j] = '\0';
        uint8_t n = atoi(param_b); 

        strcpy(param_a,"tz");
        get_param(param_a);
        ntpen = get_ntp(y,m,d,h,n,param_a);

        strcpy(param_a,"msg");
        get_param(param_a);
        strcpy(msg,param_a);
    }
    set_led(0,0,255,false);
   
    i2c_ssd1306_set_display_contrast(dev_hdl, 0xff);
    i2c_ssd1306_clear_display(dev_hdl, false);
    if(!dsp_state)dsp_state = NOC;
    
    uint8_t prev_dsp = 0;
    while (true) {   
        time(&now);    
        localtime_r(&now, &timeinfo);

        if(pipc->alrm == 3){
            if(pipc->ticks > ALRM_DELAY){
                if(pipc->con)dsp_state = CON;
                else dsp_state = NOC;
                pipc->alrm = 0;
            }
        }else{
            if(pipc->con)dsp_state = CON;
            else if(!config)dsp_state = NOC;    
            if(wen && (dsp_state != WEB))dsp_state = WEB;
            if(pipc->alrm == 2){
                pipc->ticks = 0;
                pipc->alrm = 3;
                dsp_state = ALR;
            }      
        }
        if(wen)wen--;
 
        switch(dsp_state){
            case(CFG):{
                if(prev_dsp != dsp_state){
                    prev_dsp = dsp_state;
                    set_led(200,172,0,true);
                    i2c_ssd1306_clear_display(dev_hdl, false);
                    i2c_ssd1306_set_hardware_scroll(dev_hdl,I2C_SSD1306_SCROLL_STOP,I2C_SSD1306_SCROLL_2_FRAMES);
                    strcpy(param_b," CONFIGURATION");
                    i2c_ssd1306_display_text(dev_hdl,0,param_b,strlen(param_b), false);
                    strcpy(param_b,"  MODE  ");
                    i2c_ssd1306_display_text_x2(dev_hdl, 2,param_b,strlen(param_b), false);    
                }
                break;
            }
            case(NOC):{
                if(prev_dsp != dsp_state){
                    prev_dsp = dsp_state;
                    set_led(255,255,255,true);
                }
                i2c_ssd1306_clear_display(dev_hdl, false);
                i2c_ssd1306_set_hardware_scroll(dev_hdl,I2C_SSD1306_SCROLL_STOP,I2C_SSD1306_SCROLL_2_FRAMES);
                strcpy(param_b,"Wifi:");
                if(pipc->sta)strcat(param_b,"STA ");
                else strcat(param_b,"AP  ");
                strcat(param_b,"SD:");
                if(pipc->sden)strcat(param_b,"Yes");
                else strcat(param_b,"No");
                i2c_ssd1306_display_text(dev_hdl,0,param_b, strlen(param_b), false);
                strcpy(param_b,pipc->ip);
                if(ntpen)strcat(param_b," Y");
                else strcat(param_b," N");
                i2c_ssd1306_display_text(dev_hdl,1,param_b,strlen(param_b), false);
                strftime(param_b, sizeof(param_b), "%Y%m%d:%H%M%S", &timeinfo);
                i2c_ssd1306_display_text(dev_hdl,2,param_b, strlen(param_b), false);
                strcpy(param_b,"NOT CONNECTED ");
                if(pipc->mden){
                    uint8_t len = strlen(param_b);
                    param_b[len] = save[ps];
                    param_b[len+1] = '\0';
                    if(prev_fc != pipc->fc){
                        prev_fc = pipc->fc;    
                        if(ps < 3)ps++;
                        else ps = 0;
                    }
                }
                i2c_ssd1306_display_text(dev_hdl,3,param_b, strlen(param_b), false);
                break;
            }
            case(CON):{
                if(prev_dsp != dsp_state){
                    prev_dsp = dsp_state;
                    set_led(0,255,0,true);
                    i2c_ssd1306_clear_display(dev_hdl, false);
                    i2c_ssd1306_set_hardware_scroll(dev_hdl,I2C_SSD1306_SCROLL_LEFT,I2C_SSD1306_SCROLL_2_FRAMES);
                    strcpy(param_b,"SYSTEM ACTIVE");
                    i2c_ssd1306_display_text(dev_hdl,0,param_b, strlen(param_b), false);
                    i2c_ssd1306_display_text(dev_hdl,2,msg, strlen(msg), false); 
                }   
                break;
            }
            case(ALR):{
                if(prev_dsp != dsp_state){
                    prev_dsp = dsp_state;
                    set_led(255,0,0,true);
                    i2c_ssd1306_clear_display(dev_hdl, false);
                    i2c_ssd1306_set_hardware_scroll(dev_hdl,I2C_SSD1306_SCROLL_LEFT,I2C_SSD1306_SCROLL_2_FRAMES);
                    i2c_ssd1306_display_text_x3(dev_hdl, 1, "ALARM",5, false);   
                }
                break;
            }
            case(WEB):{
                if(prev_dsp != dsp_state){
                    prev_dsp = dsp_state;
                    set_led(255,32,0,true);
                    i2c_ssd1306_clear_display(dev_hdl, false);
                    i2c_ssd1306_set_hardware_scroll(dev_hdl,I2C_SSD1306_SCROLL_STOP,I2C_SSD1306_SCROLL_2_FRAMES);
                    strcpy(param_b," REMOTE ENABLED ");
                    i2c_ssd1306_display_text(dev_hdl,0,param_b,strlen(param_b), false);
                    strcpy(param_b,"   WEB");
                    i2c_ssd1306_display_text_x2(dev_hdl, 2,param_b,strlen(param_b), false);  
                }
                break;  
            }
        }
        
        strftime(param_b, sizeof(param_b), "%H", &timeinfo);
        pipc->mden = durmap[atoi(param_b)];            
        
        uint32_t h = heap_caps_get_free_size(MALLOC_CAP_8BIT);
#ifdef DEBUG
        ESP_LOGW(TAG,"Free memory %lu Sden %d Con %d Alrm %d",h,pipc->sden,pipc->con,pipc->alrm);
#endif
        if(h < 0xFFFFF){
            ESP_LOGE(TAG,"Memory run out rebooting the system");
            esp_restart(); 
        }
        vTaskDelay(1000/portTICK_PERIOD_MS); 
    }
}   
