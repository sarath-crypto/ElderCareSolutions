#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Ticker.h>
#include <IRsend.h>
#include <ESP8266WebServer.h>

#include "LittleFS.h"

#define BUF_LEN 128
#define STA_RETRY 32
#define PORT 8881

#define AP_SSID "IRSYS_AP"
#define AP_PASS "IPSYS_PASS"

#define IR_LED 4

//192.168.0.107@irkey
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
IRsend irsend(IR_LED);

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

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);
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
      digitalWrite(BUILTIN_LED, HIGH);
      delay(250);
      digitalWrite(BUILTIN_LED, LOW);
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
  irsend.begin();
  digitalWrite(BUILTIN_LED, HIGH);
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

      if (rkey == key) {
        rxp = 0xff;
        IPAddress rip;
        digitalWrite(BUILTIN_LED, LOW);
        delay(25);
        digitalWrite(BUILTIN_LED, HIGH);
        delay(100);
        digitalWrite(BUILTIN_LED, LOW);
        delay(25);
        digitalWrite(BUILTIN_LED, HIGH);

        i = rmsg.lastIndexOf(' ');
        String cmd = rmsg.substring(i + 1);
        cmd.trim();
        uint8_t ac_cmd[10] = { 0x33, 0x28, 0x80, 0x16, 0x3B, 0x3B, 0x3B, 0x11, 0x00, 0x00 };
        if (cmd.equals("ON")) {
          ac_cmd[2] = 0x80;
          ac_cmd[9] = 0x4C;
          irsend.sendVoltas(ac_cmd);
#ifdef DEBUG
          Serial.printf("IR SEND ON\n");
#endif
        } else if (cmd.equals("OFF")) {
          ac_cmd[2] = 0x00;
          ac_cmd[9] = 0xCC;
          irsend.sendVoltas(ac_cmd);
#ifdef DEBUG
          Serial.printf("IR SEND OFF\n");
#endif
        }
      } else if (led.equals("on")) {
        digitalWrite(BUILTIN_LED, HIGH);
        delay(25);
        digitalWrite(BUILTIN_LED, LOW);
        if (!ap_mode) {
          delay(100);
          digitalWrite(BUILTIN_LED, HIGH);
          delay(25);
          digitalWrite(BUILTIN_LED, LOW);
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