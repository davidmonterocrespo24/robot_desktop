#include "Arduino.h"

#include "EmuHw.h"

EmuHw emuHw;
HardwareSerial Serial;

static bool pinOk(int pin) { return pin >= 0 && pin < EMU_PINS; }

// ---- time: the simulated clock, so the emulator can pause or fast-forward -------
unsigned long millis() { return (unsigned long)emuHw.clockMs; }
unsigned long micros() { return (unsigned long)(emuHw.clockMs * 1000.0); }
void delay(unsigned long ms) { emuHw.clockMs += (double)ms; }
void delayMicroseconds(unsigned int us) { emuHw.clockMs += us / 1000.0; }
void yield() {}

// ---- pins ---------------------------------------------------------------------------
void pinMode(int, int) {}
int digitalRead(int pin) { return pinOk(pin) ? emuHw.digitalIn[pin] : LOW; }
void digitalWrite(int pin, int value) {
  if (pinOk(pin)) emuHw.digitalOut[pin] = value;
}
int analogRead(int pin) {
  if (!pinOk(pin)) return 0;
  // a real ADC is never perfectly still
  int v = emuHw.analogIn[pin] + (rand() % 25) - 12;
  return v < 0 ? 0 : (v > 4095 ? 4095 : v);
}

// ---- misc ---------------------------------------------------------------------------
long random(long howbig) { return howbig > 0 ? rand() % howbig : 0; }
long random(long howsmall, long howbig) { return howbig > howsmall ? howsmall + rand() % (howbig - howsmall) : howsmall; }
void randomSeed(unsigned long seed) { srand((unsigned)seed); }
long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// ---- LEDC: only the tone of the buzzer matters here ----------------------------------
double ledcSetup(uint8_t, double freq, uint8_t) { return freq; }
void ledcAttachPin(uint8_t, uint8_t) {}
void ledcDetachPin(uint8_t) {}
double ledcWriteTone(uint8_t, double freq) {
  emuHw.buzzerHz = (int)freq;
  return freq;
}
void ledcWrite(uint8_t, uint32_t duty) {
  if (duty == 0) emuHw.buzzerHz = 0;
}

// ---- Serial -------------------------------------------------------------------------
void HardwareSerial::begin(unsigned long) {}
int HardwareSerial::available() { return (int)emuHw.serialIn.size(); }
int HardwareSerial::read() {
  if (emuHw.serialIn.empty()) return -1;
  int c = (unsigned char)emuHw.serialIn[0];
  emuHw.serialIn.erase(0, 1);
  return c;
}
void HardwareSerial::out(const char *s) {
  fputs(s, stdout);
  emuHw.serialOut += s;
  if (emuHw.serialOut.size() > 4000) emuHw.serialOut.erase(0, emuHw.serialOut.size() - 2000);
}
size_t HardwareSerial::print(const char *s) {
  out(s);
  return strlen(s);
}
size_t HardwareSerial::print(char c) {
  char b[2] = {c, 0};
  out(b);
  return 1;
}
size_t HardwareSerial::print(int v) {
  char b[24];
  snprintf(b, sizeof(b), "%d", v);
  return print(b);
}
size_t HardwareSerial::print(unsigned int v) {
  char b[24];
  snprintf(b, sizeof(b), "%u", v);
  return print(b);
}
size_t HardwareSerial::print(long v) {
  char b[24];
  snprintf(b, sizeof(b), "%ld", v);
  return print(b);
}
size_t HardwareSerial::print(unsigned long v) {
  char b[24];
  snprintf(b, sizeof(b), "%lu", v);
  return print(b);
}
size_t HardwareSerial::print(float v, int digits) { return print((double)v, digits); }
size_t HardwareSerial::print(double v, int digits) {
  char b[40];
  snprintf(b, sizeof(b), "%.*f", digits, v);
  return print(b);
}
size_t HardwareSerial::println() {
  out("\n");
  fflush(stdout);
  return 1;
}
