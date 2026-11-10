// Arduino API for Windows: just enough for the sketch to build and run inside
// the emulator. Inputs and outputs go to EmuHw.
#ifndef Arduino_h
#define Arduino_h

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HIGH 0x1
#define LOW 0x0
#define INPUT 0x01
#define OUTPUT 0x03
#define INPUT_PULLUP 0x05
#define INPUT_PULLDOWN 0x09

#define PROGMEM
#define F(x) (x)
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

typedef uint8_t byte;
typedef bool boolean;

unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
void yield();

void pinMode(int pin, int mode);
int digitalRead(int pin);
void digitalWrite(int pin, int value);
int analogRead(int pin);

long random(long howbig);
long random(long howsmall, long howbig);
void randomSeed(unsigned long seed);
long map(long x, long in_min, long in_max, long out_min, long out_max);

// ESP32 LEDC (core 2.x API)
double ledcSetup(uint8_t channel, double freq, uint8_t resolutionBits);
void ledcAttachPin(uint8_t pin, uint8_t channel);
void ledcDetachPin(uint8_t pin);
double ledcWriteTone(uint8_t channel, double freq);
void ledcWrite(uint8_t channel, uint32_t duty);

class HardwareSerial {
 public:
  void begin(unsigned long baud);
  int available();
  int read();
  size_t print(const char *s);
  size_t print(char c);
  size_t print(int v);
  size_t print(unsigned int v);
  size_t print(long v);
  size_t print(unsigned long v);
  size_t print(float v, int digits = 2);
  size_t print(double v, int digits = 2);
  size_t println();
  template <typename T>
  size_t println(T v) {
    size_t n = print(v);
    return n + println();
  }
  size_t println(float v, int digits) { return print(v, digits) + println(); }

 private:
  void out(const char *s);
};

extern HardwareSerial Serial;

#endif
