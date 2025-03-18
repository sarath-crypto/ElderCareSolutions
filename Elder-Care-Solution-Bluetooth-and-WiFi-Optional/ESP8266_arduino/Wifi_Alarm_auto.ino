#include <Arduino.h>
#include <FS.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

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

#define AP_RETRY        16

#define PORT            8880
#define AP_SSID         "myssid"
#define AP_PASS         "mypass"
#define WIFI_KEY        "ecsys_wifi_key_1"
#define APAP_KEY        "ecsys_apap_key_1"

#define LED_KALV        14
#define LED_ALRM        12

//#define DEBUG           1

typedef struct pdu {
  unsigned char   type;
  unsigned char   len;
  unsigned char   data[PDU_MTU];
} pdu;

enum pdu_type {KAL = 1, ALM};
enum sound {BEEP, RING};

char ip[16];
unsigned char ipi = 0;

void (*reset)(void) = 0;
WiFiUDP Udp;

char trx[BUF_LEN ];
bool pdu_rdy = false;
bool scan = false;
unsigned long msec = 0;

void play(unsigned char type) {
  AudioFileSourceSPIFFS   *in = NULL;
  AudioOutputI2SNoDAC     *out = new AudioOutputI2SNoDAC();
  switch (type) {
    case (BEEP): {
        out->SetGain(4.0f);
        in = new AudioFileSourceSPIFFS("/beep.mp3");
        break;
      }
    case (RING): {
        out->SetGain(4.0f);
        in = new AudioFileSourceSPIFFS("/ring.mp3");
        break;
      }
  }
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

void setup() {
#ifdef  DEBUG
  Serial.begin(115200);
#endif
  pinMode(LED_KALV, OUTPUT);
  pinMode(LED_ALRM, OUTPUT);

  WiFi.mode(WIFI_STA);
  SPIFFS.begin();
  delay(1000);

  WiFi.begin(AP_SSID, AP_PASS);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED) {
#ifdef  DEBUG
    Serial.printf("Connecting to AP %d %d\n", WiFi.status(), retry);
#endif
    digitalWrite(LED_KALV, HIGH);
    digitalWrite(LED_ALRM, HIGH);
    delay(500);
    digitalWrite(LED_KALV, LOW);
    digitalWrite(LED_ALRM, LOW);
    delay(500);
    retry++;
    if (retry > AP_RETRY)reset();
  }
  memcpy(ip, WiFi.gatewayIP().toString().c_str(), WiFi.gatewayIP().toString().length());
  ip[WiFi.gatewayIP().toString().length()] = '\0';
#ifdef  DEBUG
  Serial.printf("AP GW IP %s MyIP %s %ddbm\n", ip, WiFi.localIP().toString().c_str(), WiFi.RSSI());
#endif

  Udp.begin((int)PORT);
  ESP.wdtDisable();
  ESP.wdtEnable(WDTM_TIMEOUT);
  ESP.wdtFeed();
  msec = millis();
  play(BEEP);
  scan  = true;
}

void loop()
{
  unsigned long wait = millis();
  while ((millis() - wait) < WAIT_TIMEOUT);
  ESP.wdtFeed();
  if ((millis() - msec) > KALV_TIMEOUT)reset();
  pdu p;

  if (scan) {
    p.type = KAL;
    memcpy(p.data, WIFI_KEY, strlen(WIFI_KEY));
    p.len = HEADER_LEN + strlen(WIFI_KEY);

    String ips(ip);
    int i = ips.lastIndexOf('.');
    ips = ips.substring(0, i + 1) + String(ipi);
    ipi++;
    memcpy(ip, ips.c_str(), ips.length());
    ip[ips.length()] = '\0';
    pdu_rdy = true;
#ifdef  DEBUG
    Serial.printf("Scanning IP %s\n", ip);
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
    if (prx->type == KAL) {
      msec = millis();
      if (msg == APAP_KEY) {
        scan = false;
        memset(ip,0,sizeof(ip));
        memcpy(ip,rip.c_str(),rip.length());
#ifdef  DEBUG
        Serial.printf("SERVER IP %s \n", ip);
#endif
      }
      p.type = KAL;
      memcpy(p.data, WIFI_KEY, strlen(WIFI_KEY));
      p.len = HEADER_LEN + strlen(WIFI_KEY);
      pdu_rdy = true;
      digitalWrite(LED_KALV, digitalRead(LED_KALV) ^ 1);
    } else if (prx->type == ALM) {
      msec = millis();
      digitalWrite(LED_ALRM, HIGH);
      play(RING);
      digitalWrite(LED_ALRM, LOW);
    }
    Udp.flush();
  }

  if (pdu_rdy) {
    pdu_rdy = false;
#ifdef  DEBUG
    Serial.printf("TX type[%d] ip[%s] port[%d] len[%d] TO %d\n", (int)p.type, ip, PORT, p.len, millis() - msec);
#endif
    Udp.beginPacket(ip, PORT);
    Udp.write((char *)&p, p.len);
    Udp.endPacket();
    memset((void *)&p, 0, sizeof(pdu));
  }
}
