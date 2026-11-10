// Desktop robot: OLED face, two servo ears and a few senses.
//
//   sensors -> Brain (mood, reactions) -> FaceEngine (emotion graph, animation)
//                                      -> Ears / Buzzer
//
// The same sketch runs on Windows through emulator/ (see README.md).
#include <Wire.h>
#include <U8g2lib.h>
#include <ESP32Servo.h>
#include <DHT.h>

#include "Config.h"
#include "src/core/Canvas.h"
#include "src/face/FaceEngine.h"
#include "src/body/Ears.h"
#include "src/body/Buzzer.h"
#include "src/brain/Sensors.h"
#include "src/brain/Brain.h"
#include "src/brain/InfoScreen.h"

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

Canvas canvas;
FaceEngine face;
Ears ears;
Buzzer buzzer;
Sensors sensors;
Brain brain;

static uint32_t nextFrame = 0;
static uint8_t shown[CANVAS_BYTES];  // what the panel is showing (page layout)
static uint8_t pages[CANVAS_BYTES];
static bool firstFrame = true;

// Sends to the panel only the 8x8 tiles that changed. Moving two eyes touches
// a small part of the screen, so this is several times faster than a full
// refresh over I2C.
static void pushDisplay() {
  canvas.toPages(pages);
  int tx0 = 16, ty0 = 8, tx1 = -1, ty1 = -1;
  for (int ty = 0; ty < 8; ty++) {
    for (int tx = 0; tx < 16; tx++) {
      if (firstFrame || memcmp(pages + ty * 128 + tx * 8, shown + ty * 128 + tx * 8, 8) != 0) {
        if (tx < tx0) tx0 = tx;
        if (tx > tx1) tx1 = tx;
        if (ty < ty0) ty0 = ty;
        if (ty > ty1) ty1 = ty;
      }
    }
  }
  if (tx1 < 0) return;  // nothing changed
  memcpy(u8g2.getBufferPtr(), pages, CANVAS_BYTES);
  u8g2.updateDisplayArea(tx0, ty0, tx1 - tx0 + 1, ty1 - ty0 + 1);
  memcpy(shown, pages, CANVAS_BYTES);
  firstFrame = false;
}

// ---- serial console -----------------------------------------------------------------
//   <expression>      go to that face (try: happy, love, furious, asleep ...)
//   act <name>        play an act (try: pacman, pong, sneeze, fly, yes, no ...)
//   say <text>        speech balloon
//   clip <name>       play a GIF clip          list    names of faces, acts and clips
//   auto on|off       autonomous behaviour     info    temperature screen
//   mute on|off       buzzer                   status  what the robot feels
static void runCommand(char *line) {
  while (*line == ' ') line++;
  if (!*line) return;

  if (!strncmp(line, "clip ", 5)) {
    int i = Clips::find(line + 5);
    if (i < 0) Serial.println("unknown clip");
    else brain.forceClip(i);
  } else if (!strncmp(line, "act ", 4)) {
    int i = Acts::find(line + 4);
    if (i < 0) Serial.println("unknown act");
    else brain.forceAct(i);
  } else if (!strncmp(line, "say ", 4)) {
    brain.say(line + 4);
  } else if (!strcmp(line, "list")) {
    for (int i = 0; i < EX_COUNT; i++) Serial.println(Expressions::name(i));
    for (int i = 0; i < Acts::count(); i++) {
      Serial.print("act ");
      Serial.println(Acts::get(i).name);
    }
    for (int i = 0; i < Clips::count(); i++) {
      Serial.print("clip ");
      Serial.println(Clips::get(i).name);
    }
  } else if (!strncmp(line, "auto ", 5)) {
    brain.setAutonomous(!strcmp(line + 5, "on"));
  } else if (!strncmp(line, "mute ", 5)) {
    buzzer.setMuted(!strcmp(line + 5, "on"));
  } else if (!strcmp(line, "info")) {
    brain.requestInfo();
  } else if (!strcmp(line, "status")) {
    const SensorState &s = sensors.state();
    Serial.print("face=");
    Serial.print(Expressions::name(face.current()));
    Serial.print(" mood=");
    Serial.print(Expressions::moodName(brain.mood()));
    Serial.print(" doing=");
    Serial.print(brain.activityName());
    Serial.print(" (");
    Serial.print(brain.reason());
    Serial.print(")");
    Serial.print(" social=");
    Serial.print(brain.social());
    Serial.print(" annoy=");
    Serial.print(brain.annoy());
    Serial.print(" | pir=");
    Serial.print(s.motion ? 1 : 0);
    Serial.print(" ldr=");
    Serial.print(s.lightRaw);
    Serial.print(" light=");
    Serial.print(s.light);
    Serial.print(" temp=");
    Serial.print(s.tempC);
    Serial.print(" hum=");
    Serial.println(s.humidity);
  } else {
    uint8_t e = Expressions::find(line);
    if (e == EX_NONE) Serial.println("unknown command (try: list)");
    else brain.force(e, 8000);
  }
}

static void readSerial() {
  static char line[40];
  static uint8_t len = 0;
  while (Serial.available() > 0) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      line[len] = 0;
      runCommand(line);
      len = 0;
    } else if (len < sizeof(line) - 1) {
      line[len++] = ch;
    }
  }
}

// -------------------------------------------------------------------------------------
void setup() {
  Serial.begin(SERIAL_BAUD);

  u8g2.setBusClock(OLED_I2C_HZ);
  u8g2.begin();
  u8g2.setContrast(OLED_CONTRAST);
  u8g2.clearBuffer();
  u8g2.sendBuffer();

  // Noise of a floating analog pin plus the boot time: a different
  // personality on every start.
  uint32_t seed = (uint32_t)analogRead(PIN_LDR) * 2654435761u ^ (uint32_t)micros();

  sensors.begin();
  buzzer.begin();
  ears.begin(seed ^ 0xEA75u);
  face.begin(seed, EX_NORMAL);
  brain.begin(&face, seed ^ 0xB4A1u);

  nextFrame = millis();
  Serial.println("robot ready (type 'list')");
}

void loop() {
  sensors.poll();
  readSerial();

  uint32_t now = millis();
  if ((int32_t)(now - nextFrame) < 0) return;
  nextFrame += FRAME_MS;
  if ((int32_t)(now - nextFrame) > 200) nextFrame = now;  // fell behind, resync

  sensors.update(now);
  brain.update(sensors.state(), now);
  face.update(now);

  if (brain.showInfo()) {
    const SensorState &s = sensors.state();
    InfoScreen::draw(canvas, s.tempC, s.humidity, now * 0.001f);
  } else {
    face.render(canvas);
  }
  pushDisplay();

  ears.update(face.ears(), now);

  uint8_t chirp = brain.takeChirp();
  if (chirp != CH_NONE) buzzer.play(chirp, now);
  buzzer.update(now);
}
