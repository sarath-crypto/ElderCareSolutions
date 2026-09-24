#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Ticker.h>
#include <ESP8266WebServer.h>

#include "LittleFS.h"
#include "AudioFileSourceLittleFS.h"
#include "AudioFileSourceSPIFFS.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2SNoDAC.h"

#define GPIO_WLED 5
#define GPIO_SEN 4

#define AP_SSID "ALARM_AP"
#define AP_PASS "ALARM_PASS"

#define BUF_LEN 128
#define STA_RETRY 32
#define PORT 8880

//#define DEBUG 1

String ssid;
String pass;
String nkey;
String okey;
String key;
String led;

char trx[BUF_LEN];
uint8_t rxp = 0xff;
bool wrst = false;
bool ap_mode = false;

ESP8266WebServer server(80);
WiFiUDP Udp;
Ticker timer_sec;

void (*reset)(void) = 0;
void reboot(void) {
  rxp--;
  if (!rxp) reset();
}

void handle_reconfig() {
  Serial.printf("submit %d\n", server.args());
  ssid = server.arg(0);
  pass = server.arg(1);
  nkey = server.arg(2);
  okey = server.arg(3);
  led = server.arg(4);
#ifdef DEBUG
  Serial.printf("%s %s %s %s %s\n", ssid.c_str(), pass.c_str(), nkey.c_str(), okey.c_str(), led.c_str());
#endif
  if ((okey == key) || ap_mode) {
    File ofs = LittleFS.open("/config.txt", "w");
    if (ofs) {
      String msg = "ssid = " + ssid + "\npassword = " + pass + "\nkey = " + nkey + "\nled = " + led;
      ofs.print(msg);
      ofs.close();
    }
    wrst = true;
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handle_root() {
  String msg;
  File ifs = LittleFS.open("/index.html", "r");
  if (ifs) {
    msg = ifs.readString();
    ifs.close();
  }
  server.send(200, "text/html", msg);
}

void listAllFiles(const char *dirname) {
  Serial.printf("\nListing directory: %s\n", dirname);
  Dir root = LittleFS.openDir(dirname);
  while (root.next()) {
    Serial.print("FILE: ");
    Serial.print(root.fileName());
    Serial.print("  SIZE: ");
    Serial.println(root.fileSize());
  }
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

void play_ring(bool type) {
  AudioFileSourceLittleFS *in = nullptr;
  if (type) in = new AudioFileSourceLittleFS("/ring.mp3");
  else in = new AudioFileSourceLittleFS("/reminder.mp3");
  AudioOutputI2SNoDAC *out = new AudioOutputI2SNoDAC();
  AudioGeneratorMP3 *mp3 = new AudioGeneratorMP3();
  AudioFileSourceID3 *id3 = new AudioFileSourceID3(in);
  mp3->begin(id3, out);
  while (mp3->loop())
    ;
  mp3->stop();
  delete in;
  delete mp3;
  delete out;
  delete id3;
}


void setup() {
  pinMode(GPIO_WLED, OUTPUT);
  pinMode(GPIO_SEN, OUTPUT);
  digitalWrite(GPIO_WLED, HIGH);
  digitalWrite(GPIO_SEN, LOW);

  timer_sec.attach(1, reboot);
#ifdef DEBUG
  Serial.begin(115200);
#endif
  if (!LittleFS.begin()) {
    LittleFS.format();
    ap_mode = true;
  }

  if (!ap_mode) {
#ifdef DEBUG
    listAllFiles("/");
#endif

    File ifs = LittleFS.open("/config.txt", "r");
    if (ifs) {
      String line = ifs.readStringUntil('\n');
      ssid = extractValue(line);
      line = ifs.readStringUntil('\n');
      pass = extractValue(line);
      line = ifs.readStringUntil('\n');
      key = extractValue(line);
      line = ifs.readStringUntil('\n');
      led = extractValue(line);
#ifdef DEBUG
      Serial.printf("config: %s %s %s %s\n", ssid.c_str(), pass.c_str(), key.c_str(), led.c_str());
#endif
    } else ap_mode = true;
  }
  if (!ap_mode) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED) {
#ifdef DEBUG
      Serial.printf("Connecting to AP %s %s %d %d\n", ssid.c_str(), pass.c_str(), WiFi.status(), retry);
#endif
      digitalWrite(GPIO_WLED, HIGH);
      delay(250);
      digitalWrite(GPIO_WLED, LOW);
      delay(250);
      retry++;
      if (retry >= STA_RETRY) {
        ap_mode = true;
        break;
      }
    }
#ifdef DEBUG
    Serial.printf("STA GW IP %s MyIP %s %ddbm\n", WiFi.gatewayIP().toString().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
#endif
  }
  if (ap_mode) {
#ifdef DEBUG
    Serial.println("AP mode");
#endif
    IPAddress apip(10, 10, 10, 1);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(apip, apip, IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_SSID, AP_PASS, 7, false, 2);
  }
  server.on("/reconfig", HTTP_POST, handle_reconfig);
  server.on("/", HTTP_GET, handle_root);
  server.begin();
  Udp.begin(PORT);
  wifi_set_sleep_type(LIGHT_SLEEP_T);
}


void loop() {
  server.handleClient();
  if (wrst) {
#ifdef DEBUG
    Serial.printf("Web Reset\n");
#endif
    delay(5000);
    reset();
  }
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
      rxp = 0xff;
      for (int i = 0; i < 4; i++) {
        Udp.beginPacket(Udp.remoteIP(), PORT);
        String smsg = key + " OK";
        Udp.write(smsg.c_str(), smsg.length());
        bool r = Udp.endPacket();
#ifdef DEBUG
        Serial.printf("Packet transmitted status %d\n", r);
#endif
        Udp.flush();
#ifdef DEBUG
        Serial.printf("TX %s[%s]\n", smsg.c_str(), Udp.remoteIP().toString().c_str());
#endif
        delay(100);
      }

      i = rmsg.lastIndexOf(' ');
      String cmd = rmsg.substring(i + 1);
      cmd.trim();
      
      if (cmd.equals("alarm")) {
#ifdef DEBUG
        Serial.printf("RING\n");
#endif
        digitalWrite(GPIO_WLED, HIGH);
        digitalWrite(GPIO_SEN, HIGH);
        play_ring(true);
        digitalWrite(GPIO_WLED, LOW);
        digitalWrite(GPIO_SEN, LOW);
      } else if (cmd.equals("reminder")) {
#ifdef DEBUG
        Serial.printf("REMINDER \n");
#endif
        digitalWrite(GPIO_WLED, HIGH);
        digitalWrite(GPIO_SEN, HIGH);
        play_ring(false);
        digitalWrite(GPIO_WLED, LOW);
        digitalWrite(GPIO_SEN, LOW);
      } else if (led.equals("on")) {
        digitalWrite(GPIO_WLED, HIGH);
        delay(25);
        digitalWrite(GPIO_WLED, LOW);
        if (!ap_mode) {
          delay(100);
          digitalWrite(GPIO_WLED, HIGH);
          delay(25);
          digitalWrite(GPIO_WLED, LOW);
        }
      }
      while (Udp.parsePacket() > 0) {
        while (Udp.available()) {
          Udp.read();
        }
      }
    }
  }
}
