// AirCurve: enclosure exhaust controller for a 16 MHz Arduino Uno R3.
#include <Arduino.h>
#include <math.h>
#include <util/atomic.h>
#include "control.h"
#if !defined(__AVR_ATmega328P__) || F_CPU != 16000000UL
#error Select Arduino Uno R3 (ATmega328P, 16 MHz).
#endif

constexpr uint8_t PWM_PIN = 9, TACH_PIN = 2, BUTTON = 4;
constexpr uint16_t PWM_TOP = 639;  // Timer1 fast PWM: 16 MHz / 640.
volatile uint32_t edges = 0;
Controller ctl;
uint32_t tempAt, tachAt, buttonAt, rpm;
uint8_t duty = 100;
bool manual = false;
bool rawButton = HIGH, stableButton = HIGH;
float cabinetRaw = NAN, roomRaw = NAN;

void countEdge() { ++edges; }

// U1 inverts OC1A: LOW on D9 drives the fan PWM input HIGH (full speed).
void driveFan(uint8_t pct) {
  if (pct == 100) {
    TCCR1A &= ~_BV(COM1A1);
    digitalWrite(PWM_PIN, LOW);
  } else {
    OCR1A = (uint32_t)(PWM_TOP + 1) * (100 - pct) / 100 - 1;
    TCCR1A |= _BV(COM1A1);
  }
  duty = pct;
}

float readProbe(uint8_t pin) {
  analogRead(pin); // Discard first conversion after channel change.
  unsigned sum = 0;
  for (uint8_t i = 0; i < 8; ++i) sum += analogRead(pin);
  return ac_probe_c(sum / 8.0f);
}

void sampleProbes(uint32_t now) {
  cabinetRaw = readProbe(A0);
  roomRaw = readProbe(A1);
  ac_temperature(&ctl, now, cabinetRaw, roomRaw);
}

void readButton(uint32_t now) {
  bool v = digitalRead(BUTTON);
  if (v != rawButton) { buttonAt = now; rawButton = v; }
  if (v != stableButton && now - buttonAt >= 30) {
    stableButton = v;
    if (!v) manual = !manual;
  }
}

void setup() {
  digitalWrite(PWM_PIN, LOW); pinMode(PWM_PIN, OUTPUT);
  TCCR1A = _BV(WGM11); TCCR1B = 0; TCNT1 = 0; ICR1 = PWM_TOP;
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);
  pinMode(TACH_PIN, INPUT); // External 10k pull-up to Uno 5V.
  pinMode(BUTTON, INPUT_PULLUP); pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  Serial.println(F("ms,cabinet_c,ambient_c,duty_pct,rpm,mode,probe_fault,tach_fault,cabinet_raw_c,ambient_raw_c,hot,boost_requested"));
  tachAt = tempAt = millis();
  attachInterrupt(digitalPinToInterrupt(TACH_PIN), countEdge, FALLING);
  ac_init(&ctl, tempAt);
  sampleProbes(tempAt);
}

bool sampleTachIfDue(uint32_t now) {
  uint32_t elapsed = now - tachAt;
  if (elapsed < 1000) return false;
  uint32_t n;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { n = edges; edges = 0; }
  rpm = (uint64_t)n * 30000 / elapsed; // Two falling edges per revolution.
  ac_tach(&ctl, tachAt, rpm);
  tachAt = now;
  return true;
}

const __FlashStringHelper *modeName(AcMode mode) {
  switch (mode) {
    case AC_TACH:  return F("TACH");
    case AC_START: return F("START");
    case AC_PROBE: return F("PROBE");
    case AC_HOT:   return F("HOT");
    case AC_BOOST: return F("BOOST");
    case AC_AUTO:  return F("AUTO");
  }
  return F("UNKNOWN");
}

void reportTelemetry(uint32_t now, AcMode mode) {
  Serial.print(now); Serial.print(','); Serial.print(ctl.cabinet, 1); Serial.print(',');
  Serial.print(ctl.room, 1); Serial.print(','); Serial.print(duty); Serial.print(',');
  Serial.print(rpm); Serial.print(',');
  Serial.print(modeName(mode));
  Serial.print(','); Serial.print(ctl.probe_fault); Serial.print(','); Serial.print(ctl.tach_fault);
  Serial.print(','); Serial.print(cabinetRaw, 1); Serial.print(','); Serial.print(roomRaw, 1);
  Serial.print(','); Serial.print(ctl.hot);
  Serial.print(','); Serial.println(manual);
}

void loop() {
  uint32_t now = millis();
  bool report = sampleTachIfDue(now);
  readButton(now);
  if (now - tempAt >= AC_SAMPLE_MS) {
    tempAt = now;
    sampleProbes(now);
  }
  AcCommand cmd = ac_command(&ctl, now, manual);
  if (cmd.duty_pct != duty) driveFan(cmd.duty_pct);
  digitalWrite(LED_BUILTIN, ctl.probe_fault || ctl.tach_fault);
  if (report) reportTelemetry(now, cmd.mode);
}
