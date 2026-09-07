#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Ticker.h>

#include "LittleFS.h"

#include "AudioFileSourceLittleFS.h"
#include "AudioFileSourceSPIFFS.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2SNoDAC.h"

#define GPIO_WLED 5
#define GPIO_ENBL 4

#define BUF_LEN 128
#define STA_RETRY 32
#define PORT 8880

//#define DEBUG   1


String ip;
String ssid;
String pass;
String key;

AsyncWebServer server(80);
WiFiUDP Udp;

Ticker timer_sec;
char trx[BUF_LEN];

unsigned char rxp = 0;
bool wrst = false;

const char *PARAM_INPUT_1 = "input1";
const char *PARAM_INPUT_2 = "input2";
const char *PARAM_INPUT_3 = "input3";
const char *PARAM_INPUT_4 = "input4";
const char *PARAM_INPUT_5 = "input5";

const char s1[] PROGMEM = { "<!DOCTYPE HTML><html><head><title>PWRSYS Configuration</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head><body><form action=\"/get\">______WiFi_SSID<input type=\"text\" name=\"input1\" value=\"" };
const char s2[] PROGMEM = { "\"><br><br>WiFi_PASSWORD<input type=\"text\" name=\"input2\" value=\"" };
const char s3[] PROGMEM = { "\"><br><br>_______OLD_KEY<input type=\"text\" name=\"input3\" value=\"" };
const char s4[] PROGMEM = { "\"><br><br>______NEW_KEY<input type=\"text\" name=\"input4\" value=\"" };
const char s5[] PROGMEM = { "\"><br><br>______________IP<input type=\"text\" name=\"input5\" value=\"" };
const char s6[] PROGMEM = { "\"><input type=\"submit\" value=\"Reboot\"></form></body></html>" };
String index_html;

void (*reset)(void) = 0;

void reboot(void) {
  rxp--;
  if (!rxp) reset();
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Invalid Request");
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

void play_ring(void) {
  AudioFileSourceLittleFS *in = new AudioFileSourceLittleFS("/ring.mp3");
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
  pinMode(GPIO_ENBL, OUTPUT);
#ifdef DEBUG
  Serial.begin(115200);
#endif
  if (!LittleFS.begin()) {
    digitalWrite(GPIO_WLED, HIGH);
    while (1) delay(1000);
  }

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
    ip = extractValue(line);
    ifs.close();
  } else {
    digitalWrite(GPIO_WLED, HIGH);
    while (1) delay(1000);
  }
#ifdef DEBUG
  Serial.printf("config: %s %s %s %s\n", ssid.c_str(), pass.c_str(), key.c_str(), ip.c_str());
#endif
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
      digitalWrite(GPIO_WLED, HIGH);
      while (1) delay(1000);
    }
  }
#ifdef DEBUG
  Serial.printf("STA GW IP %s MyIP %s %ddbm\n", WiFi.gatewayIP().toString().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
#endif
  timer_sec.attach(1, reboot);
  Udp.begin((int)PORT);
  wifi_set_sleep_type(LIGHT_SLEEP_T);
  digitalWrite(GPIO_ENBL, LOW);
}


void loop() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    index_html = String(s1) + ssid + String(s2) + pass + String(s3) + String(s4) + String(s5) + ip + String(s6);
    request->send_P(200, "text/html", index_html.c_str());
  });
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
    String okey, nkey;
    if (request->hasParam(PARAM_INPUT_1)) {
      ssid = request->getParam(PARAM_INPUT_1)->value();
    }
    if (request->hasParam(PARAM_INPUT_2)) {
      pass = request->getParam(PARAM_INPUT_2)->value();
    }
    if (request->hasParam(PARAM_INPUT_3)) {
      okey = request->getParam(PARAM_INPUT_3)->value();
    }
    if (request->hasParam(PARAM_INPUT_4)) {
      nkey = request->getParam(PARAM_INPUT_4)->value();
    }
    if (request->hasParam(PARAM_INPUT_5)) {
      ip = request->getParam(PARAM_INPUT_5)->value();
    }
#ifdef DEBUG
    Serial.printf("Web message %s %s %s %s %s %s\n", ssid.c_str(), pass.c_str(), okey.c_str(), nkey.c_str(), ip.c_str(), key.c_str());
#endif
    if (okey == key) {
      File ofs = LittleFS.open("/config.txt", "w");
      if (ofs) {
        String msg = "ssid = " + ssid + "\npassword = " + pass + "\nkey = " + nkey + "\nip = " + ip;
        ofs.print(msg);
        ofs.close();
      }
      String web = "http://" + WiFi.localIP().toString();
      Serial.println(web.c_str());
      request->redirect(web.c_str());
      wrst = true;
    }
  });
  server.onNotFound(notFound);
  server.begin();

  while (1) {
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
        IPAddress rip;
        if (rip.fromString(ip)) {
          Udp.beginPacket(rip, PORT);
          String smsg = key + " OK";
          Udp.write(smsg.c_str(), smsg.length());
          bool r = Udp.endPacket();
#ifdef DEBUG
          if (r == 1) {
            Serial.println("Packet transmitted successfully.");
          } else {
            Serial.println("Packet transmission failed.");
          }
#endif
          Udp.flush();
#ifdef DEBUG
          Serial.printf("TX %s[%s]\n", smsg.c_str(), ip.c_str());
#endif
        }
        i = rmsg.lastIndexOf(' ');
        String cmd = rmsg.substring(i + 1);
        cmd.trim();
        if (cmd.equals("alarm")) {
#ifdef DEBUG
          Serial.printf("RING\n");
#endif
          digitalWrite(GPIO_WLED, HIGH);
          digitalWrite(GPIO_ENBL, HIGH);
          play_ring();
          digitalWrite(GPIO_WLED, LOW);
          digitalWrite(GPIO_ENBL, LOW);
          Udp.flush();
        }
      }
    }
    if (wrst) {
#ifdef DEBUG
      Serial.printf("Web Reset\n");
#endif
      delay(5000);
      reset();
    }

    digitalWrite(GPIO_WLED, HIGH);
    delay(25);
    digitalWrite(GPIO_WLED, LOW);
    delay(100);
    digitalWrite(GPIO_WLED, HIGH);
    delay(25);
    digitalWrite(GPIO_WLED, LOW);
    delay(2000);
  }
}
