/*
PB5 - 1 Reset
PB3 - 2 ADC
PB4 - 3 LED
GND - 4
PB0 - 5 SDA
PB1 - 6 UART/TRIG
PB2 - 7 SCL
VCC - 8
*/

#include <SendOnlySoftwareSerial.h>
#include <Wire.h>
#include <MPU6050.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>

#define LED_PIN PB4
#define UART_PIN PB1
#define TRIG_PIN PB1
#define ADC_PIN PB3

#define BAT_SCALE 170
#define A_THRESHOLD 20000
#define G_THRESHOLD 1000
#define BAT_THRESHOLD 2.0
#define ALARM_TO 40
#define REPEAT 8

//#define DEBUG 1
#ifdef DEBUG
SendOnlySoftwareSerial mySerial(UART_PIN);
#endif

MPU6050 mpu;
int16_t ax, ay, az;
int16_t gx, gy, gz;

int16_t pax, pay, paz;
int16_t pgx, pgy, pgz;

uint8_t tmr = 0;
bool alarm = false;


ISR(WDT_vect) {
  tmr++;
}

int8_t i2cscan(void) {
  byte error, address;
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (!Wire.endTransmission()) return address;
  }
  return 0;
}

void setup_watchdog_500ms() {
  cli();
  wdt_reset();
  WDTCR |= (1 << WDCE) | (1 << WDE);
  WDTCR = (1 << WDIE) | (1 << WDP2) | (1 << WDP0);
  sei();
}

void enter_sleep_500ms() {
  ADCSRA &= ~(1 << ADEN);
  setup_watchdog_500ms();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu();
  sleep_disable();
  ADCSRA |= (1 << ADEN);
  wdt_disable();
}

void writeOpenCollector(bool state) {
  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN,LOW);
  delay(40);
  pinMode(TRIG_PIN, INPUT);
}

void setup() {
  Wire.begin();
#ifdef DEBUG
  mySerial.begin(9600);
#endif
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  while (i2cscan() != 0x68)
    ;
  delay(1000);
  digitalWrite(LED_PIN, LOW);
  delay(1000);
  digitalWrite(LED_PIN, HIGH);
  delay(1000);
  digitalWrite(LED_PIN, LOW);
}

void loop() {
  float vbat = (float)analogRead(ADC_PIN) / (float)BAT_SCALE;
  mpu.getAcceleration(&ax, &ay, &az);
  mpu.getRotation(&gx, &gy, &gz);
  double adiff = abs(ax - pax) + abs(ay - pay) + abs(az - paz);
  double gdiff = abs(gx - pgx) + abs(gy - pgy) + abs(gz - pgz);
  pax = ax;
  pay = ay;
  paz = ay;
  pgx = gx;
  pgy = gy;
  pgz = gz;

#ifdef DEBUG
  mySerial.printf("%d\n", (int)vbat);
#endif

  if (!alarm) {
    if (((int)abs(adiff) > A_THRESHOLD) && ((int)abs(gdiff) > G_THRESHOLD)) {
      tmr = 0;
      alarm = true;
      digitalWrite(LED_PIN, HIGH);
      for (uint8_t i = 0; i < REPEAT; i++) {
        writeOpenCollector(true);
        delay(40);
        writeOpenCollector(true);
        delay(40);
      }
    }
  } else if (tmr >= ALARM_TO) {
    alarm = false;
    digitalWrite(LED_PIN, LOW);
    pax = ax;
    pay = ay;
    paz = ay;
    pgx = gx;
    pgy = gy;
    pgz = gz;
    delay(1000);
  }
  if (vbat <= BAT_THRESHOLD) {
    digitalWrite(LED_PIN, HIGH);
    delay(25);
    digitalWrite(LED_PIN, LOW);
    delay(100);
    digitalWrite(LED_PIN, HIGH);
    delay(25);
    digitalWrite(LED_PIN, LOW);
  }
#ifndef DEBUG
  enter_sleep_500ms();
#endif
}
