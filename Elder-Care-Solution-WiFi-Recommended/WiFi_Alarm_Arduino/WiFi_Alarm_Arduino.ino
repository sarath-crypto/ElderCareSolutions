#include <Arduino.h>
#include <FS.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "AudioFileSourceSPIFFS.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2SNoDAC.h"

#define  PDU_MTU        1450
#define  HEADER_LEN     2
#define  BUF_LEN        1452

#define KALV_TIMEOUT    60000
#define WDTM_TIMEOUT    20000
#define WAIT_TIMEOUT    100

#define STA_RETRY       16
#define PORT            8880

#define LED_KALV        5
#define LED_ALRM        4

#define AP_SSID         "ECSYS_AP"
#define AP_PASS         "ECSYS_PASS"

//#define FORMAT          1
//#define DEBUG           1

typedef struct pdu {
  unsigned char   type;
  unsigned char   len;
  unsigned char   data[PDU_MTU];
} pdu;

enum pdu_type {KAL = 1, ALM};

char gwip[16];
unsigned char ipi = 0;

String ssid;
String pass;
String key;

const char* PARAM_INPUT_1 = "input1";
const char* PARAM_INPUT_2 = "input2";
const char* PARAM_INPUT_3 = "input3";

const char s1[] PROGMEM = {"<!DOCTYPE HTML><html><head><title>ECSYS Configuration</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head><body><form action=\"/get\">______WiFi_SSID:<input type=\"text\" name=\"input1\" value=\""};
const char s2[] PROGMEM = {"\"><br><br>WiFi_PASSWORD:<input type=\"text\" name=\"input2\" value=\""};
const char s3[] PROGMEM = {"\"><br><br>____________KEY:<input type=\"text\" name=\"input3\" value=\""};
const char s4[] PROGMEM = {"\"><input type=\"submit\" value=\"Reboot\"></form></body></html>"};
String index_html;

void (*reset)(void) = 0;

WiFiUDP Udp;

char trx[BUF_LEN ];
bool pdu_rdy = false;
bool scan = false;
unsigned long msec = 0;

void play_ring(void) {
  AudioFileSourceSPIFFS   *in = new AudioFileSourceSPIFFS("/ring.mp3");
  AudioOutputI2SNoDAC     *out = new AudioOutputI2SNoDAC();
  AudioGeneratorMP3       *mp3 = new AudioGeneratorMP3();
  AudioFileSourceID3      *id3 = new AudioFileSourceID3(in);
  mp3->begin(id3, out);
  while (mp3->loop());
  mp3->stop();
  delete in;
  delete mp3;
  delete out;
  delete id3;
}

String get_value(String line, String key) {
  int lpos = 0;
  String item;
  do {
    lpos = line.indexOf('\n');
    item = line.substring(0, lpos);
    int ipos = item.indexOf('=');
    String param = item.substring(0, ipos);
    if (param == key) {
      int vpos = item.indexOf('=');
      key = item.substring(vpos + 1);
      break;
    }
    lpos++;
    line = line.substring(lpos);
  } while (lpos);
  return key;
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Invalid Request");
}

void setup() {
#ifdef  DEBUG
  Serial.begin(115200);
#endif
  pinMode(LED_KALV, OUTPUT);
  pinMode(LED_ALRM, OUTPUT);
  digitalWrite(LED_KALV, HIGH);
  digitalWrite(LED_ALRM, HIGH);

  SPIFFS.begin();
  delay(1000);

#ifdef FORMAT
  SPIFFS.format();
#ifdef  DEBUG
  Serial.printf("Format completed\n");
#endif
#endif
  int  p = 0;
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    String fn = dir.fileName();
    if (fn == "/ring.mp3")p++;
    if (fn == "/config.txt")p++;
#ifdef  DEBUG
    Serial.printf("Files %s %d\n", fn.c_str(), p);
#endif
  }
  if (p != 2)reset();
  File fin = SPIFFS.open("/config.txt", "r");
  if (!fin)reset();
  String line;
  while (fin.available()) {
    char c = fin.read();
    if ((c == '\n') || (c == '\r')) {
      if (line[line.length() - 1] != '\n')line += '\n';
      continue;
    }
    if (c != ' ')line += c;
  }
  fin.close();
  ssid = get_value(line, "ssid");
  pass = get_value(line, "password");
  key = get_value(line, "key");

#ifdef  DEBUG
  Serial.printf("config: %s %s %s\n", ssid.c_str(), pass.c_str(), key.c_str());
#endif

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED) {
#ifdef  DEBUG
    Serial.printf("Connecting to AP %s %s %d %d\n", ssid.c_str(), pass.c_str(), WiFi.status(), retry);
#endif
    digitalWrite(LED_KALV, HIGH);
    digitalWrite(LED_ALRM, HIGH);
    delay(500);
    digitalWrite(LED_KALV, LOW);
    digitalWrite(LED_ALRM, LOW);
    delay(500);
    retry++;
    if (retry > STA_RETRY) break;
  }
  if (retry > STA_RETRY) {
    IPAddress apip(10, 10, 10, 1);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(apip, apip, IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_SSID, AP_PASS, 7, false, 1);

    index_html = String(s1) + ssid + String(s2) + pass + String(s3) + key + String(s4);
    AsyncWebServer server(80);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send_P(200, "text/html", index_html.c_str());
    });
    server.on("/get", HTTP_GET, [] (AsyncWebServerRequest * request) {
      if (request->hasParam(PARAM_INPUT_1)) {
        ssid = request->getParam(PARAM_INPUT_1)->value();
      }
      if (request->hasParam(PARAM_INPUT_2)) {
        pass = request->getParam(PARAM_INPUT_2)->value();
      }
      if (request->hasParam(PARAM_INPUT_3)) {
        key = request->getParam(PARAM_INPUT_3)->value();
      }
#ifdef  DEBUG
      Serial.printf("Web message %s %s %s\n", ssid.c_str(), pass.c_str(), key.c_str());
#endif
      File fout = SPIFFS.open("/config.txt", "w");
      if (!fout)reset();
      ssid = "ssid = " + ssid;
      pass =  "password = " + pass;
      key = "key = " + key;
      fout.println(ssid.c_str());
      fout.println(pass.c_str());
      fout.println(key.c_str());
      fout.close();
      reset();
    });
    server.onNotFound(notFound);
    server.begin();

    while (1) {
      digitalWrite(LED_KALV, HIGH);
      digitalWrite(LED_ALRM, LOW);
      delay(500);
      digitalWrite(LED_KALV, LOW);
      digitalWrite(LED_ALRM, HIGH);
      delay(500);
#ifdef  DEBUG
      Serial.printf("Webserver waiting\n");
#endif
    }
  } else {
    memcpy(gwip, WiFi.gatewayIP().toString().c_str(), WiFi.gatewayIP().toString().length());
    gwip[WiFi.gatewayIP().toString().length()] = '\0';
#ifdef  DEBUG
    Serial.printf("AP GW IP %s MyIP %s %ddbm\n", gwip, WiFi.localIP().toString().c_str(), WiFi.RSSI());
#endif
    Udp.begin((int)PORT);
    ESP.wdtDisable();
    ESP.wdtEnable(WDTM_TIMEOUT);
    ESP.wdtFeed();
    msec = millis();
    scan  = true;
  }
}

void loop()
{
  unsigned long wait = millis();
  while ((millis() - wait) < WAIT_TIMEOUT);
  ESP.wdtFeed();
  if ((millis() - msec) > KALV_TIMEOUT) {
#ifdef  DEBUG
    Serial.printf("Reseting %d %d\n", (millis() - msec), KALV_TIMEOUT);
#endif
    reset();
  }
  pdu p;

  if (scan) {
    p.type = KAL;
    memcpy(p.data, key.c_str(), key.length());
    p.len = HEADER_LEN + key.length();

    String ips(gwip);
    int i = ips.lastIndexOf('.');
    ips = ips.substring(0, i + 1) + String(ipi);
    ipi++;
    memcpy(gwip, ips.c_str(), ips.length());
    gwip[ips.length()] = '\0';
    pdu_rdy = true;
#ifdef  DEBUG
    Serial.printf("Scanning IP %s Key %s\n", gwip, key.c_str());
#endif
  }

  int pktsz = Udp.parsePacket();
  if (pktsz) {
    int n = Udp.read(trx, BUF_LEN);
    pdu *prx = (pdu *)&trx;
    char buf[64];
    memcpy(buf, prx->data, prx->len - HEADER_LEN);
    buf[prx->len - HEADER_LEN] = '\0';
    String msg = String(buf);
    String rip = Udp.remoteIP().toString();
#ifdef  DEBUG
    Serial.printf("RX %s type %d %d %s\n", rip.c_str(), (int)prx->type, (int)prx->len, msg.c_str());
#endif
    if (msg == key) {
      if (prx->type == KAL) {
        msec = millis();
        scan = false;
        memset(gwip, 0, sizeof(gwip));
        memcpy(gwip, rip.c_str(), rip.length());
#ifdef  DEBUG
        Serial.printf("KAL SERVER IP %s %s %d %d\n", gwip, msg.c_str(), (millis() - msec), KALV_TIMEOUT);
#endif
        p.type = KAL;
        memcpy(p.data, key.c_str(), key.length());
        p.len = HEADER_LEN + key.length();
        pdu_rdy = true;
        digitalWrite(LED_KALV, digitalRead(LED_KALV) ^ 1);
      } else if (prx->type == ALM) {
        msec = millis();
        digitalWrite(LED_ALRM, HIGH);
        play_ring();
        digitalWrite(LED_ALRM, LOW);
      }
    }
    Udp.flush();
  }

  if (pdu_rdy) {
    pdu_rdy = false;
#ifdef  DEBUG
    char buf[64];
    memcpy(buf, p.data, p.len - HEADER_LEN);
    buf[p.len - HEADER_LEN] = '\0';
    String msg = String(buf);
    Serial.printf("TX type[%d] ip[%s] port[%d] len[%d] msg[%s] TO %d\n", (int)p.type, gwip, PORT, p.len, msg.c_str(), millis() - msec);
#endif
    Udp.beginPacket(gwip, PORT);
    Udp.write((char *)&p, p.len);
    Udp.endPacket();
    memset((void *)&p, 0, sizeof(pdu));
  }
}
