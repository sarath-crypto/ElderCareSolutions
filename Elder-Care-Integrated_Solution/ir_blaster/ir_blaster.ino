#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Ticker.h>
#include <IRsend.h>

#include "LittleFS.h"

#define BUF_LEN 128
#define STA_RETRY 32
#define PORT 8883

#define IR_LED 4

//192.168.0.1@my_key
//#define DEBUG 1


String ip;
String ssid;
String pass;
String key;

AsyncWebServer server(80);
WiFiUDP Udp;
IRsend irsend(IR_LED);
Ticker timer_sec;

char trx[BUF_LEN];
unsigned char rxp = 0xff;
bool wrst = false;

const char *PARAM_INPUT_1 = "input1";
const char *PARAM_INPUT_2 = "input2";
const char *PARAM_INPUT_3 = "input3";
const char *PARAM_INPUT_4 = "input4";
const char *PARAM_INPUT_5 = "input5";

const char s1[] PROGMEM = { "<!DOCTYPE HTML><html><head><title>IRSYS Configuration</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head><body><form action=\"/get\">______WiFi_SSID<input type=\"text\" name=\"input1\" value=\"" };
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


void setup() {
  pinMode(BUILTIN_LED, OUTPUT);
  timer_sec.attach(1, reboot);
#ifdef DEBUG
  Serial.begin(115200);
#endif
  if (!LittleFS.begin()) {
    digitalWrite(BUILTIN_LED, HIGH);
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
    digitalWrite(BUILTIN_LED, LOW);
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
    digitalWrite(BUILTIN_LED, LOW);
    delay(250);
    digitalWrite(BUILTIN_LED, HIGH);
    delay(250);
    retry++;
    if (retry >= STA_RETRY) {
      digitalWrite(BUILTIN_LED, LOW);
      while (1) delay(1000);
    }
  }
#ifdef DEBUG
  Serial.printf("STA GW IP %s MyIP %s %ddbm\n", WiFi.gatewayIP().toString().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
#endif

  Udp.begin((int)PORT);
  wifi_set_sleep_type(LIGHT_SLEEP_T);
  irsend.begin();
  digitalWrite(BUILTIN_LED, HIGH);
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
          i = rmsg.lastIndexOf(' ');
          String cmd = rmsg.substring(i + 1);
          cmd.trim();

          if (cmd.equals("alive")) {
            digitalWrite(BUILTIN_LED, LOW);
            delay(500);
            digitalWrite(BUILTIN_LED, HIGH);
          } else {
            uint8_t ac_cmd[10] = { 0x33, 0x28, 0x80, 0x16, 0x3B, 0x3B, 0x3B, 0x11, 0x00, 0x00 };
            if (cmd.equals("ON")) {
              ac_cmd[2] = 0x80;
              ac_cmd[9] = 0x4C;
            } else if (cmd.equals("OFF")) {
              ac_cmd[2] = 0x00;
              ac_cmd[9] = 0xCC;
            }
            irsend.sendVoltas(ac_cmd);
#ifdef DEBUG
            Serial.printf("IR SEND\n");
#endif
          }
          while (Udp.parsePacket() > 0) {
            while (Udp.available()) {
              Udp.read();
            }
          }
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

    digitalWrite(BUILTIN_LED, LOW);
    delay(25);
    digitalWrite(BUILTIN_LED, HIGH);
    delay(100);
    digitalWrite(BUILTIN_LED, LOW);
    delay(25);
    digitalWrite(BUILTIN_LED, HIGH);
    delay(2000);
  }
}
