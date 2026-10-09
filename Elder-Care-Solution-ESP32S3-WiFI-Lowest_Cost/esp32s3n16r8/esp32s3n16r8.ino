#include <Arduino.h>

#include "FS.h"
#include "aviwriter.h"
#include "SD_MMC.h"
#include <LittleFS.h>

#include <Ticker.h>
#include <ESP32Time.h>
#include <EspUsbHost.h>
#include <Adafruit_NeoPixel.h>
#include <assert.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRac.h>
#include <IRtext.h>
#include <IRutils.h>
#include <IRsend.h>
#include <base64.h>
#include <algorithm>
#include <TimeLib.h>

#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include "esp_camera.h"
#include "mbedtls/base64.h"

#include "EPD_3in7g.h"
#include "GUI_Paint.h"
#include "fonts.h"
#include "ImageData.h"

#include "time.h"
#include <WiFiUdp.h>
#include <WiFi.h>
#include "esp32s3n16r8.h"
#include <JPEGDEC.h>

#include <vector>
#include <algorithm>

//#define DEBUG 1

String ssid, pass, key, aip, scr, bto, nmode, achrs, cona, conb, acon, acoff, mouse, flip, rem, epoch;
String clk, dn, motion;

uint8_t ac_map[24];
uint8_t nm_map[24];
uint8_t rm_map[24];
float adc_avg[16];

uint8_t acs = AC_IDLE;
uint8_t bl = 100;
uint8_t bp = 0;
uint8_t r = 0, g = 0, b = 0;
uint8_t rxp = 0;
bool curr_ac = false;
bool prev_rm = false;
bool ext_pwr = false;
bool conn = false;
bool prev_conn = false;
bool ap_mode = false;
bool ntp = false;
bool wrst = false;
bool prev_alrm = false;
bool alrm = false;
bool camera_init = false;
bool refresh = false;
bool rec = false;

uint8_t asec = 0;
uint8_t bsec = 0;
uint16_t adc = 0;
uint16_t rsec = 0;
uint16_t fc = 0;

char trx[BUF_LEN];
int ipi = 0;

decode_results ir_data;
IRsend irsend(IR_TX_LED);
Adafruit_NeoPixel pixels(1, RGB_LED, NEO_GRB + NEO_KHZ800);
WiFiUDP Udp;
AsyncWebServer server(80);
web_data wd;
JPEGDEC jpeg;
ESP32Time rtc(0);
Ticker timer_sec;
EspUsbHost usbHost;
aviwriter avi;

File logFile;
SemaphoreHandle_t logMutex;

QueueHandle_t msgQueue_web;
QueueHandle_t msgQueue_md;
std::vector<camera_fb_t *> mptr, wptr;

void mouse_Callback(const EspUsbHostMouseEvent &mouseEvent) {
  if (alrm) return;
  else {
    String path = "/" + rtc.getTime("%y%m%d%H%M%S") + ".trg";
    File file = SD_MMC.open(path.c_str(), FILE_WRITE);
    if (file) file.close();
    else esp_log_write(ESP_LOG_ERROR, "ECSYS", "%lu mouse_callback trg file creation failed\n", rtc.getLocalEpoch());
    if (mouseEvent.buttons & 0x01) alrm = true;
    if (mouseEvent.buttons & 0x02) alrm = true;
    if (mouseEvent.buttons & 0x04) alrm = true;
    if (mouseEvent.wheel != 0) alrm = true;
    if ((mouse.equals("high") && (mouseEvent.x != 0 || mouseEvent.y != 0))) alrm = true;
  }
}

void sec_timer(void) {
#ifdef DEBUG
  uint32_t startTime = millis();
#endif
  if (alrm) asec++;
  if (asec >= ALARM_TO) {
    alrm = false;
    asec = 0;
  }
  if (conn) rxp++;
  if (rxp > CONN_TO) conn = false;
  bsec++;
  rsec++;
  camera_fb_t *fb = nullptr;
  if (camera_init) {
    fb = esp_camera_fb_get();
    if (fb) {
      if (xQueueSend(msgQueue_md, &fb, pdMS_TO_TICKS(10)) != pdPASS) esp_camera_fb_return(fb);
    }
  }
#ifdef DEBUG
  uint32_t executionTime = millis() - startTime;
  Serial.printf("Ticks (~40) %d ms %d\n", executionTime, (fb) ? fb->len : 0);
#endif
}

void rotateLogFile() {
  if (logFile) logFile.close();
  if (LittleFS.exists(OLD_FILE_PATH)) LittleFS.remove(OLD_FILE_PATH);
  if (LittleFS.exists(LOG_FILE_PATH)) LittleFS.rename(LOG_FILE_PATH, OLD_FILE_PATH);
  logFile = LittleFS.open(LOG_FILE_PATH, "a");
}

String urlDecode(String str) {
  String decoded = "";
  char ch;
  int i = 0;

  while (i < str.length()) {
    ch = str.charAt(i);
    if (ch == '%') {
      if (i + 2 < str.length()) {
        char hex1 = str.charAt(i + 1);
        char hex2 = str.charAt(i + 2);
        char decodedChar = (char)((hex2bin(hex1) << 4) | hex2bin(hex2));
        decoded += decodedChar;
        i += 3;
      } else {
        decoded += ch;
        i++;
      }
    } else if (ch == '+') {
      decoded += ' ';
      i++;
    } else {
      decoded += ch;
      i++;
    }
  }
  return decoded;
}

std::byte hex2bin(char c) {
  if (c >= '0' && c <= '9') return ((std::byte)(c - '0'));
  if (c >= 'A' && c <= 'F') return ((std::byte)(c - 'A' + 10));
  if (c >= 'a' && c <= 'f') return ((std::byte)(c - 'a' + 10));
  return ((std::byte)0);
}

void hex2Byte(const char *hexString, std::byte *byteArray) {
  int len = strlen(hexString);
  for (int i = 0; i < len; i += 2) {
    std::byte hn = hex2bin(hexString[i]);
    std::byte ln = hex2bin(hexString[i + 1]);
    byteArray[i / 2] = (hn << 4) | ln;
  }
}

bool isNumeric(String str) {
  str.trim();
  if (str.length() == 0) return false;
  int startIdx = 0;
  if (str.charAt(0) == '-' || str.charAt(0) == '+') {
    if (str.length() == 1) return false;
    startIdx = 1;
  }
  for (int i = startIdx; i < str.length(); i++) {
    if (!isDigit(str.charAt(i))) {
      return false;
    }
  }
  return true;
}

String extractValue(String line) {
  int equalsIndex = line.indexOf('=');
  if (equalsIndex == -1) {
    return "";
  }
  String value = line.substring(equalsIndex + 1);
  value.trim();
  return value;
}


void listAllFiles(fs::FS &fs, const char *dirname) {
#ifdef DEBUG
  if (&fs == &LittleFS) {
    Serial.println("This is LittleFS");
    Serial.printf("Total space: %u MB\n", LittleFS.totalBytes() / (1024 * 1024));
    Serial.printf("Used space:  %u MB\n", LittleFS.usedBytes() / (1024 * 1024));
    Serial.printf("Free space:  %u MB\n", (LittleFS.totalBytes() - LittleFS.usedBytes()) / (1024 * 1024));
  } else if (&fs == &SD_MMC) {
    Serial.println("This is SD Card");
    Serial.printf("SD_MMC Card Size: %llu MB Type:%d \n", SD_MMC.cardSize() / (1024 * 1024), SD_MMC.cardType());
    Serial.printf("Total space: %llu MB\n", SD_MMC.totalBytes() / (1024 * 1024));
    Serial.printf("Used space: %llu MB\n", SD_MMC.usedBytes() / (1024 * 1024));
    Serial.printf("Free space:  %u MB\n", (SD_MMC.totalBytes() - SD_MMC.usedBytes()) / (1024 * 1024));
  }
  Serial.printf("Listing directory: %s\n", dirname);
#endif
  File root = fs.open(dirname);
  if (!root) {
    esp_log_write(ESP_LOG_ERROR, "ECSYS", "%lu Failed to open directory\n", rtc.getLocalEpoch());
    return;
  }
  if (!root.isDirectory()) {
    esp_log_write(ESP_LOG_ERROR, "ECSYS", "%lu Not a directory\n", rtc.getLocalEpoch());
    return;
  }
  File file = root.openNextFile();
  while (file) {
#ifdef DEBUG
    Serial.printf("%s %d Bytes\n", file.name(), file.size());
#endif
    String fn(file.name());
    if (fn.endsWith(".avi") && !file.size()) SD_MMC.remove((String("/") + String(file.name())).c_str());
    file = root.openNextFile();
  }
}

void SD_cleanup(uint8_t sz) {
  size_t freeBytes = SD_MMC.totalBytes() - SD_MMC.usedBytes();
  File root = SD_MMC.open("/");
  if (!root) {
    esp_log_write(ESP_LOG_ERROR, "ECSYS", "%lu Failed to open directory\n", rtc.getLocalEpoch());
    return;
  }
  if (!root.isDirectory()) {
    esp_log_write(ESP_LOG_ERROR, "ECSYS", "%lu Not a directory\n", rtc.getLocalEpoch());
    return;
  }
  std::vector<epfn> vepfn;
  File file = root.openNextFile();
  while (file) {
    epfn tdata;
    tdata.fn = String(file.name());
    ;
    tmElements_t ts;
    ts.Year = 2000 + tdata.fn.substring(0, 2).toInt() - 1970;
    ts.Month = tdata.fn.substring(2, 4).toInt();
    ts.Day = tdata.fn.substring(4, 6).toInt();
    ts.Hour = tdata.fn.substring(6, 8).toInt();
    ts.Minute = tdata.fn.substring(8, 10).toInt();
    ts.Second = tdata.fn.substring(10, 12).toInt();
    tdata.epoch = makeTime(ts);
    vepfn.push_back(tdata);
    file = root.openNextFile();
  }
  std::sort(vepfn.begin(), vepfn.end(), [](const epfn &a, const epfn &b) {
    return a.epoch < b.epoch;
  });

  while (freeBytes < (sz * 1000000)) {
    String fn = "/" + vepfn[0].fn;
    SD_MMC.remove(fn.c_str());
    freeBytes = SD_MMC.totalBytes() - SD_MMC.usedBytes();
  }
}

int log_vprintf(const char *fmt, va_list args) {
  char log_buffer[256];
  int len = vsnprintf(log_buffer, sizeof(log_buffer), fmt, args);
  if (len > 0) {
    if (logMutex != NULL && xSemaphoreTake(logMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      if (logFile) {
        if (logFile.size() + len > LOG_FILE_SIZE) rotateLogFile();
        if (logFile) {
          logFile.print(log_buffer);
          logFile.flush();
        }
      }
      xSemaphoreGive(logMutex);
    }
  }
  return len;
}

String process_ac(void) {
  String ticks;
  std::vector<uint16_t> du16;
  for (uint16_t i = 1; i < ir_data.rawlen; i++) {
    uint32_t usecs;
    for (usecs = ir_data.rawbuf[i] * kRawTick; usecs > UINT16_MAX; usecs -= UINT16_MAX) {
      ticks += uint64ToString(UINT16_MAX);
      if (i % 2) ticks += F(", 0,  ");
      else ticks += F(",  0, ");
    }
    ticks += uint64ToString(usecs, 10);
    du16.push_back(ticks.toInt());
    ticks.clear();
  }
  String cmd = base64::encode((uint8_t *)du16.data(), du16.size() * 2);
  return cmd;
}


String index_handler(void) {
  String msg;
  File ifs = LittleFS.open("/index.html", "r");
  if (ifs) {
    msg = ifs.readString();
    ifs.close();
  }
  return msg;
}

void add_events(String &msg, String label, uint8_t val) {
  msg += "<div class = \"bar-wrapper\"><div class = \"bar\" style = \"height: " + String(val) + "%;\"><span class = \" label \"> " + label + "</span></div></div>\n";
}

String events_handler(void) {
  String msg;
  File ifs = LittleFS.open("/events.html", "r");
  if (ifs) {
    msg = ifs.readString();
    ifs.close();
  }
  File root = SD_MMC.open("/");
  if (!root) {
    esp_log_write(ESP_LOG_ERROR, "ECSYS", "%lu events_handler SD_MMC.open() failed\n", rtc.getLocalEpoch());
    return msg;
  }
  if (!root.isDirectory()) {
    esp_log_write(ESP_LOG_ERROR, "ECSYS", "%lu events_handler root.isDirectory() failed\n", rtc.getLocalEpoch());
    return msg;
  }
  File file = root.openNextFile();
  while (file) {
    String fn = file.name();
    if (fn.endsWith(".avi")) add_events(msg, String(file.name()), file.size() / 1024);
    file = root.openNextFile();
  }
  msg += "</div></div><div class=\"graph-container\"><h2>Alarm Triggerings</h2><div class=\"chart bottom-graph\">\n";
  root.rewindDirectory();
  file = root.openNextFile();
  while (file) {
    String fn = file.name();
    if (fn.endsWith(".trg")) add_events(msg, String(file.name()), 10);
    file = root.openNextFile();
  }
  msg += "\n</div></div></body></html>";
  return msg;
}

String camera_handler(void) {
  String msg;
  File ifs;

  if (flip.equals("no")) ifs = LittleFS.open("/camerah.html", "r");
  else ifs = LittleFS.open("/camerav.html", "r");

  if (ifs) {
    msg = ifs.readString();
    ifs.close();
  }
  return msg;
}

void setVerticalFlip(bool flip) {
  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor) sensor->set_vflip(sensor, flip);
}

void setHorizontalMirror(bool mirror) {
  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor) sensor->set_hmirror(sensor, mirror);
}

void setBrightness(int level) {
  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor && level >= -2 && level <= 2) {
    sensor->set_brightness(sensor, level);
  }
}

void setSaturation(int level) {
  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor && level >= -2 && level <= 2) {
    sensor->set_saturation(sensor, level);
  }
}

void setContrast(int level) {
  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor && level >= -2 && level <= 2) {
    sensor->set_contrast(sensor, level);
  }
}

int JPEGDrawCallback(JPEGDRAW *pDraw) {
  uint8_t *grayPixels = (uint8_t *)pDraw->pPixels;
  uint32_t j = 0;
  for (int y = 0; y < pDraw->iHeight; y++) {
    for (int x = 0; x < pDraw->iWidth; x++, j++) {
      wd.pnbw[j] = grayPixels[y * pDraw->iWidth + x];
    }
  }
  return 1;
}

bool init_cam(void) {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 8000000;
  config.frame_size = FRAMESIZE_VGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 15;
  config.fb_count = IPCQ_SZ;

  if (!psramFound()) {
#ifdef DEBUG
    Serial.println("PSRAM  NOT FOUND - Optimized settings");
#endif

    return false;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
#ifdef DEBUG
    Serial.printf("Camera init failed with error 0x%x\n", err);
#endif
    return false;
  }

  sensor_t *sensor = esp_camera_sensor_get();
#ifdef DEBUG
  Serial.printf("Camera init %02x\n", sensor->id.PID);
#endif
  camera_init = true;
  return true;
}

// Task on Core 0: Producer
void epr_Task(void *pvParameters) {
  UBYTE *Image;
  UWORD Imagesize = ((EPD_3IN7G_WIDTH % 4 == 0) ? (EPD_3IN7G_WIDTH / 4) : (EPD_3IN7G_WIDTH / 4 + 1)) * EPD_3IN7G_HEIGHT;
  if ((Image = (UBYTE *)malloc(Imagesize)) == NULL) {
    vTaskDelete(NULL);
  }
  DEV_Module_Init();
  uint16_t angle = 90;
  if (flip.equals("no")) angle = 270;

  while (true) {
    if (refresh) {
#ifdef DEBUG
      Serial.println("Refresh");
#endif
      uint16_t days = (millis() / (1000 * 60 * 60) % 24) / 24;

      EPD_3IN7G_Init_Fast();
      Paint_NewImage(Image, EPD_3IN7G_WIDTH, EPD_3IN7G_HEIGHT, angle, EPD_3IN7G_WHITE);
      Paint_SetScale(4);
      Paint_SelectImage(Image);
      Paint_Clear(EPD_3IN7G_WHITE);
      ///416x240
      String label = "Emergency Contacts:" + cona;
      Paint_DrawString_EN(5, 0, label.c_str(), &Font20, EPD_3IN7G_WHITE, EPD_3IN7G_RED);
      Paint_DrawString_EN(270, 20, conb.c_str(), &Font20, EPD_3IN7G_WHITE, EPD_3IN7G_RED);
      switch (acs) {
        case (AC_ON):
          {
            Paint_DrawRectangle(0, 40, 416, 172, EPD_3IN7G_YELLOW, DOT_PIXEL_1X1, DRAW_FILL_FULL);
            Paint_DrawString_EN(10, 45, "Press AC ON", &Font40, EPD_3IN7G_YELLOW, EPD_3IN7G_BLACK);
            Paint_DrawString_EN(10, 85, "Switch", &Font40, EPD_3IN7G_YELLOW, EPD_3IN7G_BLACK);
            break;
          }
        case (AC_OFF):
          {
            Paint_DrawRectangle(0, 40, 416, 172, EPD_3IN7G_YELLOW, DOT_PIXEL_1X1, DRAW_FILL_FULL);
            Paint_DrawString_EN(10, 45, "Press AC OFF", &Font40, EPD_3IN7G_YELLOW, EPD_3IN7G_BLACK);
            Paint_DrawString_EN(10, 85, "Switch", &Font40, EPD_3IN7G_YELLOW, EPD_3IN7G_BLACK);
            break;
          }
        case (AC_IDLE):
          {
            Paint_DrawRectangle(0, 40, 416, 172, EPD_3IN7G_RED, DOT_PIXEL_1X1, DRAW_FILL_FULL);
            if (alrm) {
              Paint_DrawString_EN(10, 45, "ALARM", &Font96, EPD_3IN7G_RED, EPD_3IN7G_YELLOW);
            } else if (!conn) {
              Paint_DrawString_EN(10, 45, "Alarm System", &Font40, EPD_3IN7G_RED, EPD_3IN7G_YELLOW);
              Paint_DrawString_EN(40, 100, "Not Connected", &Font30, EPD_3IN7G_RED, EPD_3IN7G_YELLOW);
            } else {
              Paint_DrawRectangle(0, 40, 416, 172, EPD_3IN7G_YELLOW, DOT_PIXEL_1X1, DRAW_FILL_FULL);
              label = rtc.getDate(true);
              Paint_DrawString_EN(5, 50, label.c_str(), &Font20, EPD_3IN7G_YELLOW, EPD_3IN7G_BLACK);
              uint8_t hr = rtc.getHour();
              if (hr < 10) label = " " + String(hr);
              else label = String(hr);
              label += ":" + String(rtc.getMinute());
              Paint_DrawString_EN(5, 75, label.c_str(), &Font80, EPD_3IN7G_YELLOW, EPD_3IN7G_BLACK);
              Paint_DrawString_EN(340, 110, rtc.getAmPm().c_str(), &Font40, EPD_3IN7G_YELLOW, EPD_3IN7G_BLACK);
            }
            break;
          }
      }
      label = "Access Point:" + ssid;
      Paint_DrawString_EN(0, 172, label.c_str(), &Font12, EPD_3IN7G_WHITE, EPD_3IN7G_BLACK);
      label = "Password:" + pass;
      Paint_DrawString_EN(0, 184, label.c_str(), &Font12, EPD_3IN7G_WHITE, EPD_3IN7G_BLACK);
      label = "My IP Address:" + WiFi.localIP().toString();
      Paint_DrawString_EN(0, 196, label.c_str(), &Font12, EPD_3IN7G_WHITE, EPD_3IN7G_BLACK);
      label = "Alarm IP Address:" + aip;
      Paint_DrawString_EN(0, 208, label.c_str(), &Font12, EPD_3IN7G_WHITE, EPD_3IN7G_BLACK);

      label = "UpTime[" + String(days) + "]Days";
      Paint_DrawString_EN(0, 220, label.c_str(), &Font20, EPD_3IN7G_WHITE, EPD_3IN7G_RED);
      if (ext_pwr) label = "POWER";
      else label = "BAT:" + String((int)bl) + "%";
      Paint_DrawString_EN(220, 220, label.c_str(), &Font20, EPD_3IN7G_WHITE, EPD_3IN7G_RED);
      if (curr_ac) Paint_DrawString_EN(375, 220, "AC", &Font20, EPD_3IN7G_WHITE, EPD_3IN7G_RED);

      EPD_3IN7G_Display(Image);
      EPD_3IN7G_Sleep();
      refresh = false;
    }

    camera_fb_t *fb = nullptr;
    if (xQueueReceive(msgQueue_md, &fb, pdMS_TO_TICKS(10)) == pdPASS) {
      mptr.push_back(fb);
      if (wd.conn) {
        if (xQueueSend(msgQueue_web, &fb, pdMS_TO_TICKS(10)) != pdPASS) {
#ifdef DEBUG
          Serial.println("WEB Q Full");
#endif
        }
      }
    }

    if (!wd.conn && wptr.size()) {
      wptr.clear();
      while (xQueueReceive(msgQueue_md, &fb, pdMS_TO_TICKS(10)) == pdPASS)
        ;
    }

    if (mptr.size() > IPCQ_SZ - 1) {
      for (int i = 0; i < wptr.size(); i++)
        if (mptr[0] == wptr[i]) wptr.erase(wptr.begin() + i);
      esp_camera_fb_return(mptr[0]);
      mptr.erase(mptr.begin());
    }
    float mp = 0.0;
    if (!wd.pobw) wd.pobw = (uint8_t *)malloc(FRAME_SIZE);
    if (!wd.pnbw) wd.pnbw = (uint8_t *)malloc(FRAME_SIZE);
    if (wd.pobw && wd.pnbw && fb) {
      if (jpeg.openRAM((uint8_t *)fb->buf, fb->len, JPEGDrawCallback)) {
        jpeg.setPixelType(EIGHT_BIT_GRAYSCALE);
        if (jpeg.decode(0, 0, 0)) {
          unsigned long cp = 0;
          for (size_t i = 0; i < FRAME_SIZE; i++) {
            int diff = abs((int)wd.pnbw[i] - (int)wd.pobw[i]);
            if (diff > MOTION_THRESHOLD) cp++;
          }
          mp = ((float)cp / (float)fb->len) * 100.0;
        }
        memcpy(wd.pobw, wd.pnbw, FRAME_SIZE);
        jpeg.close();
      }
    }
    if (mp > motion.toInt()) {
      if (!rec) {
        rec = true;
        fc = 0;
        String path = "/" + rtc.getTime("%y%m%d%H%M%S") + ".avi";
        File avif = SD_MMC.open(path.c_str(), FILE_WRITE);
        if (avif) {
          if (!avi.begin(avif, FRAME_WIDTH, FRAME_HEIGHT, VIDEO_FPS, MAX_TOTAL_FRAMES)) {
            esp_log_write(ESP_LOG_ERROR, "ECSYS", "%lu avi.begin() failed\n", rtc.getLocalEpoch());
            rec = false;
          }
        } else {
          esp_log_write(ESP_LOG_ERROR, "ECSYS", "%lu avi SD_MMC.open failed %s\n", rtc.getLocalEpoch(), path.c_str());
          rec = false;
        }
      }
      if ((MIN_TOTAL_FRAMES - fc) < 10) fc -= 10;
    }
    if (rec && fb) {
      if (!avi.addFrame(fb->buf, fb->len)) fc = MIN_TOTAL_FRAMES;
      fc++;
      if (fc >= MIN_TOTAL_FRAMES) {
        avi.close();
        rec = false;
      }
    }
#ifdef DEBUG
    UBaseType_t remainingSpace = uxTaskGetStackHighWaterMark(NULL);
    unsigned int stackSizeAllocated = 3000;
    unsigned int maxUsedSoFar = stackSizeAllocated - remainingSpace;
    printf("Task epr Stack: Max Used = %u bytes, Remaining Free = %u bytes core %d\n", maxUsedSoFar, (unsigned int)remainingSpace, xPortGetCoreID());
#endif
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  DEV_Module_Exit();
}

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
#endif
  memset((void *)&wd, 0, sizeof(wd));
  pinMode(IR_TX_LED, OUTPUT);
  analogReadResolution(12);

  pixels.begin();
  pixels.clear();
  pixels.setPixelColor(0, pixels.Color(255, 0, 0));
  pixels.show();
  delay(1000);

  if (!LittleFS.begin(true)) ap_mode = true;

  logMutex = xSemaphoreCreateMutex();
  logFile = LittleFS.open(LOG_FILE_PATH, "a");
  if (!logFile) {
#ifdef DEBUG
    Serial.println("Failed to open log file!");
#endif
    return;
  }
  esp_log_level_set("*", ESP_LOG_VERBOSE);
  esp_log_set_vprintf(&log_vprintf);
  esp_log_write(ESP_LOG_INFO, "ECSYS", "%lu Booting and last reset reason %d\n", rtc.getLocalEpoch(), esp_reset_reason());

  if (!SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0)) {
#ifdef DEBUG
    Serial.println("Pin assignment failed!");
#endif
    esp_log_write(ESP_LOG_ERROR, "ECSYS", "%lu SD_MMC.setPins() failed\n", rtc.getLocalEpoch());
    return;
  }
  if (!SD_MMC.begin("/sdcard", true, true)) {
#ifdef DEBUG
    Serial.println("Card Mount/Initialization Failed");
#endif
    esp_log_write(ESP_LOG_ERROR, "ECSYS", "%lu Card Mount/Initialization Failed\n", rtc.getLocalEpoch());
    return;
  }

#ifdef DEBUG
  listAllFiles(LittleFS, "/");
#endif
  listAllFiles(SD_MMC, "/");

  String nkey, okey;
  File ifs = LittleFS.open("/config.txt", "r");
  if (ifs) {
    String line = ifs.readStringUntil('\n');
    ssid = extractValue(line);
    line = ifs.readStringUntil('\n');
    pass = extractValue(line);
    line = ifs.readStringUntil('\n');
    key = extractValue(line);
    line = ifs.readStringUntil('\n');
    nmode = extractValue(line);
    nmode = urlDecode(nmode);
    line = ifs.readStringUntil('\n');
    achrs = extractValue(line);
    achrs = urlDecode(achrs);
    line = ifs.readStringUntil('\n');
    bto = extractValue(line);
    line = ifs.readStringUntil('\n');
    scr = extractValue(line);
    line = ifs.readStringUntil('\n');
    cona = extractValue(line);
    line = ifs.readStringUntil('\n');
    conb = extractValue(line);
    line = ifs.readStringUntil('\n');
    acon = extractValue(line);
    line = ifs.readStringUntil('\n');
    acoff = extractValue(line);
    line = ifs.readStringUntil('\n');
    mouse = extractValue(line);
    line = ifs.readStringUntil('\n');
    flip = extractValue(line);
    line = ifs.readStringUntil('\n');
    motion = extractValue(line);
    line = ifs.readStringUntil('\n');
    rem = extractValue(line);
    line = ifs.readStringUntil('\n');
    epoch = extractValue(line);
    ifs.close();
  } else {
    esp_log_write(ESP_LOG_INFO, "ECSYS", "%lu LittleFS.open() failed, ap mode activated\n", rtc.getLocalEpoch());
    r = 255;
    ap_mode = true;
  }
#ifdef DEBUG
  Serial.printf("config: [%s] [%s] [%s]\n", ssid.c_str(), pass.c_str(), key.c_str());
  Serial.printf("config: [%s] [%s] [%s]\n", nmode.c_str(), achrs.c_str(), bto.c_str());
  Serial.printf("config: [%s] [%s] [%s]\n", scr.c_str(), cona.c_str(), conb.c_str());
  Serial.printf("config: [%s] [%s] [%s]\n", acon.c_str(), acoff.c_str(), mouse.c_str());
  Serial.printf("config: [%s] [%s] [%s]\n", flip.c_str(), motion.c_str(), rem.c_str());
  Serial.printf("config: [%s] \n", epoch.c_str());
#endif

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED) {
#ifdef DEBUG
    Serial.printf("Connecting to AP %s %s %d %d\n", ssid.c_str(), pass.c_str(), WiFi.status(), retry);
#endif
    pixels.clear();
    pixels.setPixelColor(0, pixels.Color(255, 255, 0));
    pixels.show();
    delay(250);
    pixels.clear();
    pixels.show();
    delay(250);
    retry++;
    if (retry >= STA_RETRY) {
      esp_log_write(ESP_LOG_WARN, "ECSYS", "%lu WiFi connecting failed\n", rtc.getLocalEpoch());
      ap_mode = true;
      break;
    }
  }
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
#ifdef DEBUG
  Serial.printf("STA GW IP %s MyIP %s %ddbm\n", WiFi.gatewayIP().toString().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
#endif
  if (ap_mode) {
    IPAddress local_ip(10, 10, 10, 1);
    IPAddress gateway(10, 10, 10, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(local_ip, gateway, subnet);
    ssid = "ECSYS_AP";
    pass = "ECSYS_PASS";
    WiFi.softAP(ssid, pass);
    ntp = false;
    time_t te = (time_t)strtoul(epoch.c_str(), NULL, 10);
    rtc.offset = GMT_OFFSET;
    rtc.setTime(te, 0);
#ifdef DEBUG
    Serial.printf("AP %s %s %s\n", ssid.c_str(), pass.c_str(), WiFi.softAPIP().toString().c_str());
#endif
    esp_log_write(ESP_LOG_WARN, "ECSYS", "%lu ap mode activated\n", rtc.getLocalEpoch());
  } else {
    configTime(GMT_OFFSET, 0, "0.pool.ntp.org");
    struct tm timeinfo;
    retry = 0;
    ntp = true;
    while (!getLocalTime(&timeinfo)) {
      if (wrst) {
        pixels.clear();
        pixels.setPixelColor(0, pixels.Color(255, 0, 255));
        pixels.show();
      } else {
        pixels.clear();
        pixels.show();
      }
      wrst = !wrst;
      retry++;
      if (retry >= (STA_RETRY / 2)) {
        ntp = false;
        break;
      }
#ifdef DEBUG
      Serial.printf("NTP %d\n", retry);
#endif
    }
    wrst = false;
    if (ntp) {
      time_t te = mktime(&timeinfo);
      rtc.setTime(te, 0);
    } else {
      esp_log_write(ESP_LOG_WARN, "ECSYS", "%lu NTP not available using preconfigured time\n", rtc.getLocalEpoch());
      time_t te = (time_t)strtoul(epoch.c_str(), NULL, 10);
      rtc.offset = GMT_OFFSET;
      rtc.setTime(te, 0);
    }
  }

  timer_sec.attach(0.5, sec_timer);
  msgQueue_web = xQueueCreate(IPCQ_SZ, sizeof(camera_fb_t *));
  if (msgQueue_web == NULL) {
#ifdef DEBUG
    Serial.println("Error creating the queue web");
#endif
    esp_log_write(ESP_LOG_ERROR, "ECSYS", "%lu Q web failed\n", rtc.getLocalEpoch());
    return;
  }
  msgQueue_md = xQueueCreate(IPCQ_SZ, sizeof(camera_fb_t *));
  if (msgQueue_md == NULL) {
#ifdef DEBUG
    Serial.println("Error creating the queue md");
#endif
    esp_log_write(ESP_LOG_ERROR, "ECSYS", "%lu Q md failed\n", rtc.getLocalEpoch());
    return;
  }

  // Pin Producer Task to Core 0
  xTaskCreatePinnedToCore(
    epr_Task,  // Function to implement the task
    "epr",     // Name of the task
    4096,      // Stack size in words
    NULL,      // Task input parameter
    0,         // Priority of the task
    NULL,      // Task handle
    0          // Core ID (1)
  );

  memset(nm_map, 0, sizeof(nm_map));
  String sbuf = nmode;
  if (sbuf.length()) {
    char *token = strtok((char *)sbuf.c_str(), ",");
    while (token != NULL) {
      String val(token);
      if (isNumeric(val)) nm_map[val.toInt()] = 0xff;
      token = strtok(NULL, ",");
    }
    sbuf.clear();
#ifdef DEBUG
    for (int i = 0; i < sizeof(nm_map); i++) {
      Serial.printf("[%d %02x]", i, nm_map[i]);
    }
    Serial.println("");
#endif
  }

  memset(rm_map, 0, sizeof(rm_map));
  sbuf = rem;
  if (sbuf.length()) {
    char *token = strtok((char *)sbuf.c_str(), ",");
    while (token != NULL) {
      String val(token);
      if (isNumeric(val)) rm_map[val.toInt()] = 0xff;
      token = strtok(NULL, ",");
    }
    sbuf.clear();
#ifdef DEBUG
    for (int i = 0; i < sizeof(rm_map); i++) {
      Serial.printf("[%d %02x]", i, rm_map[i]);
    }
    Serial.println("");
#endif
  }
  usbHost.onMouse(mouse_Callback);
  usbHost.begin();
  Udp.begin(PORT);

  if (ap_mode) g = 255;
  else b = 255;

  memset(ac_map, 0, sizeof(ac_map));
  sbuf = achrs;
  if (sbuf.length()) {
    char *token = strtok((char *)sbuf.c_str(), ",");
    while (token != NULL) {
      String val(token);
      if (isNumeric(val)) ac_map[val.toInt()] = 0xff;
      token = strtok(NULL, ",");
    }
    sbuf.clear();
#ifdef DEBUG
    for (int i = 0; i < sizeof(ac_map); i++) {
      Serial.printf("[%d %02x]", i, ac_map[i]);
    }
    Serial.println("");
#endif
  }

  irsend.begin();

  if (acon.equals("auto")) {
    acs = AC_ON;
    refresh = true;
#ifdef DEBUG
    Serial.println("AC INIT");
#endif
    esp_log_write(ESP_LOG_INFO, "ECSYS", "%lu Entered AC ON configuraion\n", rtc.getLocalEpoch());
    IRrecv irrecv(EXT_PWR, IR_BUF_LEN, AC_TO, true);
    irrecv.setTolerance(kTolerance);
    irrecv.enableIRIn();
    while (true) {
      if (irrecv.decode(&ir_data)) {
        if (ir_data.overflow) continue;
        String model = typeToString(ir_data.decode_type, ir_data.repeat);
        if (acon.equals("auto") && model.equals("UNKNOWN")) continue;
        break;
      }
      yield();
    }
    acon = process_ac();
    acs = AC_OFF;
    refresh = true;
#ifdef DEBUG
    Serial.printf("AC ON DONE %s\n", typeToString(ir_data.decode_type, ir_data.repeat).c_str());
#endif
    esp_log_write(ESP_LOG_INFO, "ECSYS", "%lu Entered AC OFF configuraion\n", rtc.getLocalEpoch());
    while (true) {
      if (irrecv.decode(&ir_data)) {
        if (ir_data.overflow) continue;
        String model = typeToString(ir_data.decode_type, ir_data.repeat);
        if (acoff.equals("auto") && model.equals("UNKNOWN")) continue;
        break;
        yield();
      }
    }
    acoff = process_ac();
#ifdef DEBUG
    Serial.printf("AC OFF DONE %s\n", typeToString(ir_data.decode_type, ir_data.repeat).c_str());
#endif
    esp_log_write(ESP_LOG_INFO, "ECSYS", "%lu AC Found %s\n", rtc.getLocalEpoch(), typeToString(ir_data.decode_type, ir_data.repeat).c_str());
    File ofs = LittleFS.open("/config.txt", "w");
    if (ofs) {
      String msg = "ssid = " + ssid + "\npassword = " + pass + "\nkey = " + key + "\nnmode = " + nmode + "\nachrs = " + achrs + "\nbto = " + bto + "\nscreen = " + scr;
      msg += "\ncona = " + cona + "\nconb = " + conb + "\nacon = " + acon + "\nacoff = " + acoff + "\nmouse = " + mouse + "\nflip = " + flip;
      msg += "\nmotion = " + motion + "\nrem = " + rem + "\nepoch = " + epoch;
#ifdef DEBUG
      Serial.printf("[%d] %s\n", msg.length(), msg.c_str());
#endif
      ofs.print(msg);
      ofs.close();
    }
  }
  acs = AC_IDLE;
  refresh = true;

  server.on("/events", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", events_handler());
  });

  server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (LittleFS.exists("/log.txt")) {
      request->send(LittleFS, "/log.txt", "text/plain");
    } else {
      request->send(404, "text/plain", "File not found");
    }
  });

  server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("file")) {
      String filepath = "/" + request->getParam("file")->value();
      if (SD_MMC.exists(filepath)) {
        request->send(SD_MMC, filepath, "application/octet-stream", true);
        return;
      }
    }
    request->send(404, "text/plain", "File Not Found");
  });

  server.on("/storage", HTTP_GET, [](AsyncWebServerRequest *request) {
    File root = SD_MMC.open("/");
    if (!root) {
      esp_log_write(ESP_LOG_ERROR, "ECSYS", "%lu Failed to open directory storage\n", rtc.getLocalEpoch());
      return;
    }
    if (!root.isDirectory()) {
      esp_log_write(ESP_LOG_ERROR, "ECSYS", "%lu Not a directory storage\n", rtc.getLocalEpoch());
      return;
    }
    String html = "<html lang='en'><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:sans-serif;margin:20px;}ul{list-style-type:none;padding:0;}li{padding:8px 0;border-bottom:1px solid #ddd;}</style>";
    html += "</head><body>";
    html += "<h1>EcSys Storage SD Card Files</h1>";
    html += "<ul>";

    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) html += "<li><a href='/download?file=" + String(file.name()) + "'>" + String(file.name()) + "</a> (" + String(file.size()) + " bytes)</li>";
      file = root.openNextFile();
    }
    html += "</ul></body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", index_handler());
  });

  server.on("/camera", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", camera_handler());
  });

  server.on("/reconfig", HTTP_POST, [](AsyncWebServerRequest *request) {
    String okey, nkey;
    if (request->hasParam("ssid", true)) ssid = request->getParam("ssid", true)->value();
    if (request->hasParam("pass", true)) pass = request->getParam("pass", true)->value();
    if (request->hasParam("nkey", true)) nkey = request->getParam("nkey", true)->value();
    if (request->hasParam("okey", true)) okey = request->getParam("okey", true)->value();
    if (request->hasParam("nmode", true)) nmode = request->getParam("nmode", true)->value();
    if (request->hasParam("achrs", true)) achrs = request->getParam("achrs", true)->value();
    if (request->hasParam("bto", true)) bto = request->getParam("bto", true)->value();
    if (request->hasParam("scr", true)) scr = request->getParam("scr", true)->value();
    if (request->hasParam("cona", true)) cona = request->getParam("cona", true)->value();
    if (request->hasParam("conb", true)) conb = request->getParam("conb", true)->value();
    if (request->hasParam("ac", true)) {
      String ac = request->getParam("ac", true)->value();
      acon = ac;
      acoff = ac;
    }
    if (request->hasParam("mouse", true)) mouse = request->getParam("mouse", true)->value();
    if (request->hasParam("flip", true)) flip = request->getParam("flip", true)->value();
    if (request->hasParam("motion", true)) motion = request->getParam("motion", true)->value();
    if (request->hasParam("rem", true)) rem = request->getParam("rem", true)->value();
    if (request->hasParam("epoch", true)) epoch = request->getParam("epoch", true)->value();

    String msg = "ssid = " + ssid + "\npassword = " + pass + "\nkey = " + nkey + "\nnmode = " + nmode + "\nachrs = " + achrs + "\nbto = " + bto + "\nscreen = " + scr;
    msg += "\ncona = " + cona + "\nconb = " + conb + "\nacon = " + acon + "\nacoff = " + acoff + "\nmouse = " + mouse + "\nflip = " + flip;
    msg += "\nmotion = " + motion + "\nrem = " + rem + "\nepoch = " + epoch;

    if (okey == key) {
#ifdef DEBUG
      Serial.println("Key OK");
#endif
      File ofs = LittleFS.open("/config.txt", "w");
      if (ofs) {
#ifdef DEBUG
        Serial.printf("%s\n", msg.c_str());
#endif
        ofs.print(msg);
        ofs.close();
      }
      wrst = true;
      request->send(200, "text/plain", "Configuration updated successfully and system is rebooting now...");
    } else {
      request->send(200, "text/plain", "Incorrect key Re-Configuration failed.");
    }
  });

  server.on("/stream", HTTP_GET, [](AsyncWebServerRequest *request) {
    wd.state = SEND_HEADER;
    wd.conn = true;

    auto clientConnected = std::make_shared<bool>(true);
    request->onDisconnect([clientConnected]() {
      wd.conn = false;
    });

    AsyncWebServerResponse *response = request->beginChunkedResponse("multipart/x-mixed-replace;boundary=" STREAM_BOUNDARY, [&wd](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
      if (!wd.conn) return 0;
      wd.conn = true;
      switch (wd.state) {
        case SEND_HEADER:
          {
            camera_fb_t *fb = wd.fb;
            if (xQueueReceive(msgQueue_web, &wd.fb, pdMS_TO_TICKS(10)) == pdPASS) {
              wptr.push_back(fb);
              wd.len = wd.fb->len;
              wd.pjpg = wd.fb->buf;
            }
            if (!wd.fb) {
              if (!wd.offline.length()) {
                File ifs = LittleFS.open("/offline.jpg", "r");
                if (ifs) {
                  wd.offline = ifs.readString();
                  ifs.close();
                }
              }
              wd.len = wd.offline.length();
              wd.pjpg = (uint8_t *)wd.offline.c_str();
            }
            char headerBuf[128];
            size_t headerLen = snprintf(headerBuf, sizeof(headerBuf), "\r\n--%s\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n", STREAM_BOUNDARY, wd.len);
            wd.bytesSent = 0;
            size_t available = headerLen - wd.bytesSent;
            size_t toWrite = (available > maxLen) ? maxLen : available;
            memcpy(buffer, headerBuf + wd.bytesSent, toWrite);
            wd.bytesSent += toWrite;
            if (wd.bytesSent >= headerLen) {
              wd.bytesSent = 0;
              wd.state = SEND_DATA;
            }
            return toWrite;
          }
        case SEND_DATA:
          {
            size_t available = wd.len - wd.bytesSent;
            size_t toWrite = (available > maxLen) ? maxLen : available;
            memcpy(buffer, wd.pjpg + wd.bytesSent, toWrite);
            wd.bytesSent += toWrite;
            if (wd.bytesSent >= wd.len) wd.state = SEND_HEADER;
            return toWrite;
          }
      }
    });
    response->addHeader("Cache-Control", "no-cache, private");
    response->addHeader("Pragma", "no-cache");
    request->send(response);
  });

  server.begin();
  pinMode(EXT_PWR, INPUT);
  if (!init_cam()) {
#ifdef DEBUG
    Serial.println("Camera failed");
#endif
    esp_log_write(ESP_LOG_ERROR, "ECSYS", "%lu init_cam() failed\n", rtc.getLocalEpoch());
    r = 255;
  }
  esp_log_write(ESP_LOG_INFO, "ECSYS", "%lu EcSys Operational\n");
}

void loop() {
  bool tx = false;
  String tx_cmd;

  int pktsz = Udp.parsePacket();
  if (pktsz) {
    int n = Udp.read(trx, BUF_LEN);
    String rmsg = String(trx).substring(0, n);
#ifdef DEBUG
    Serial.printf("RX msg %s\n", rmsg.c_str());
#endif
    int i = rmsg.indexOf(' ');
    String rkey = rmsg.substring(0, i);
    if (rkey == key) {
      rxp = 0;
      i = rmsg.lastIndexOf(' ');
      String cmd = rmsg.substring(i + 1);
      cmd.trim();
      if (cmd.equals("OK")) {
        aip = Udp.remoteIP().toString();
        conn = true;
      }
    }
  }
  if (!conn) {
    String ip = WiFi.localIP().toString();
    int pos = ip.lastIndexOf(".");
    ip = ip.substring(0, pos + 1);
    aip = ip + String(ipi);
    ipi++;
    if (ipi >= 255) ipi = 0;
    tx = true;
    tx_cmd = "alive";
  }
  if (bsec >= (bto.toInt()) * 2) {
    bsec = 0;
    if (conn) {
      tx_cmd = "alive";
      tx = true;
    }
  }

  if (!rm_map[rtc.getHour(true)]) {
    if (!prev_rm) {
      tx_cmd = "reminder";
      tx = true;
      prev_rm = true;
    }
  } else prev_rm = false;

  if (prev_alrm != alrm) {
    refresh = true;
    if (alrm) {
      tx_cmd = "alarm";
      tx = true;
    }
    prev_alrm = alrm;
  }

  if (tx) {
    tx = false;
    IPAddress rip;
    if (rip.fromString(aip)) {
      Udp.beginPacket(rip, PORT);
      String smsg = key + " " + tx_cmd;
      Udp.write((uint8_t *)smsg.c_str(), smsg.length());
      bool r = Udp.endPacket();
      Udp.flush();
#ifdef DEBUG
      Serial.printf("TX %s[%s] %d\n", smsg.c_str(), aip.c_str(), r);
#endif
    }
  }
  if (wrst) {
#ifdef DEBUG
    Serial.printf("Web Reset\n");
#endif
    esp_log_write(ESP_LOG_WARN, "ECSYS", "%lu System reset via web interface\n", rtc.getLocalEpoch());
    ESP.restart();
  }

  if (!nm_map[rtc.getHour(true)]) {
    if (rsec >= (scr.toInt() * 60) && !alrm) {
      rsec = 0;
      refresh = true;
    }
  }
  if (conn != prev_conn) {
    rsec = 0;
    refresh = true;
    prev_conn = conn;
  }

  adc = analogReadMilliVolts(ADC_BAT);
  ext_pwr = !digitalRead(EXT_PWR);

  String ac_cmd;
  if (ac_map[rtc.getHour(true)]) {
    if (!curr_ac) {
      ac_cmd = acon;
      curr_ac = true;
#ifdef DEBUG
      Serial.println("AC ON");
#endif
      esp_log_write(ESP_LOG_INFO, "ECSYS", "%lu AC switch ON\n", rtc.getLocalEpoch());
    }
  } else {
    if (curr_ac) {
      ac_cmd = acoff;
      curr_ac = false;
#ifdef DEBUG
      Serial.println("AC OFF");
#endif
      esp_log_write(ESP_LOG_INFO, "ECSYS", "%lu AC switch OFF\n", rtc.getLocalEpoch());
    }
  }

  if (ac_cmd.length()) {
    size_t dlen = 0;
    uint16_t *rawdata = (uint16_t *)malloc((ac_cmd.length() * 3) / 4);
    int result = mbedtls_base64_decode((uint8_t *)rawdata, (ac_cmd.length() * 3) / 4, &dlen, (const unsigned char *)ac_cmd.c_str(), ac_cmd.length());
#ifdef DEBUG
    Serial.printf("[%d %d] %s\n", ac_cmd.length(), dlen, ac_cmd.c_str());
#endif
    if (result != 0) free(rawdata);
    else {
      irsend.sendRaw((const uint16_t *)rawdata, dlen / 2, 38);
      free(rawdata);
    }
    ac_cmd.clear();
  }

  adc_avg[bp] = adc;
  bp++;
  if (bp >= 16) bp = 0;
  float s_adc = 0;
  for (int i = 0; i < 16; i++) s_adc += adc_avg[i];
  s_adc = (s_adc * 1.68) / (float)16000;

  if (s_adc >= 3.82) bl = 100;
  else if (s_adc >= 3.79) bl = 90;
  else if (s_adc >= 3.73) bl = 80;
  else if (s_adc >= 3.68) bl = 60;
  else if (s_adc >= 3.55) bl = 50;
  else if (s_adc >= 3.39) bl = 40;
  else if (s_adc >= 3.00) bl = 30;
  else bl = 5;

#ifdef DEBUG
  Serial.printf("Loop[%d] EXT_PWR:%d ADC:%02f %d %s ALRM %d AC %d\n", xPortGetCoreID(), ext_pwr, s_adc, bl, rtc.getTimeDate().c_str(), alrm, curr_ac);
  Serial.printf("[B:%d R:%d A:%d] Free Heap: %d bytes %d\n", bsec, rsec, asec, esp_get_free_heap_size(), camera_init);
#endif

  if ((WiFi.status() != WL_CONNECTED) && !ap_mode) {
#ifdef DEBUG
    Serial.println("WiFi connection lost! Reconnecting");
#endif
    esp_log_write(ESP_LOG_WARN, "ECSYS", "%lu WiFi connection lost,reconnecting\n", rtc.getLocalEpoch());
    WiFi.disconnect();
    WiFi.reconnect();
  }
  if (esp_get_free_heap_size() < 10000) {
    esp_log_write(ESP_LOG_WARN, "ECSYS", "%lu System reset due heap less than 10KB\n", rtc.getLocalEpoch());
    ESP.restart();
  }
  SD_cleanup(10);

  pixels.setPixelColor(0, pixels.Color(r, g, b));
  pixels.show();
  delay(10);
  pixels.clear();
  pixels.show();
  delay(490);
}
