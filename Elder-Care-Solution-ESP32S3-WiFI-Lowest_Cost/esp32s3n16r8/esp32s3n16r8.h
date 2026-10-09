#ifndef ESP32S3N16R8_H
#define ESP32S3N16R8_H

#include <Arduino.h>
#include <EspUsbHost.h>
#include <atomic>
#include "esp_camera.h"

#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 15
#define SIOD_GPIO_NUM 4
#define SIOC_GPIO_NUM 5
#define Y2_GPIO_NUM 11
#define Y3_GPIO_NUM 9
#define Y4_GPIO_NUM 8
#define Y5_GPIO_NUM 10
#define Y6_GPIO_NUM 12
#define Y7_GPIO_NUM 18
#define Y8_GPIO_NUM 17
#define Y9_GPIO_NUM 16
#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM 7
#define PCLK_GPIO_NUM 13

#define SD_MMC_CLK 39
#define SD_MMC_CMD 38
#define SD_MMC_D0 40
#define SD_MMC_D1 4
#define SD_MMC_D2 12
#define SD_MMC_D3 13

#define RGB_LED 48
#define IR_TX_LED 3
#define EXT_PWR 46
#define ADC_BAT 1

#define PORT 8880
#define STA_RETRY 32
#define AC_RETRY 60
#define GMT_OFFSET 19800
#define BUF_LEN 128
#define ALARM_TO 50
#define CONN_TO 120
#define AC_TO 60
#define IR_BUF_LEN 1024
#define STREAM_BOUNDARY "123456789000000000000987654321"
#define IPCQ_SZ 3
#define MOTION_THRESHOLD 30
#define FRAME_WIDTH 640
#define FRAME_HEIGHT 480
#define FRAME_SIZE (640 * 480)
#define VIDEO_FPS 2
#define MAX_TOTAL_FRAMES 600
#define MIN_TOTAL_FRAMES 40

#define LOG_FILE_PATH "/log.txt"
#define OLD_FILE_PATH "/log.old"
#define LOG_FILE_SIZE 0x400000

enum cmd { CMD_IDLE,
           CMD_REFRESH };

enum StreamState { SEND_HEADER,
                   SEND_DATA };
enum acs { AC_IDLE,
           AC_ON,
           AC_OFF };

typedef struct web_data {
  uint8_t *pjpg;
  uint32_t len;
  camera_fb_t *fb;
  bool conn;
  StreamState state;
  uint8_t *pobw;
  uint8_t *pnbw;
  size_t bytesSent;
  String offline;
} web_data;

typedef struct epfn {
  uint32_t epoch;
  String fn;
} epfn;


#endif