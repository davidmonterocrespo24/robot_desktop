// U8g2 for the emulator. The sketch draws on its own Canvas and uses U8g2 only
// as the driver of the panel, so this is all that is needed: a buffer and the
// calls that send it. What is "sent" lands in EmuHw::panel.
#ifndef U8G2LIB_HH
#define U8G2LIB_HH

#include <stdint.h>
#include <string.h>

#include "EmuHw.h"

#define U8G2_R0 0
#define U8G2_R1 1
#define U8G2_R2 2
#define U8G2_R3 3
#define U8X8_PIN_NONE 255

class U8G2_SSD1306_128X64_NONAME_F_HW_I2C {
 public:
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C(int /*rotation*/, uint8_t /*reset*/ = U8X8_PIN_NONE,
                                       uint8_t /*clock*/ = U8X8_PIN_NONE, uint8_t /*data*/ = U8X8_PIN_NONE) {
    memset(buf_, 0, sizeof(buf_));
  }

  void setBusClock(uint32_t hz) { emuHw.i2cHz = hz; }
  bool begin() {
    memset(emuHw.panel, 0, sizeof(emuHw.panel));
    return true;
  }
  void setContrast(uint8_t v) { emuHw.contrast = v; }
  void setPowerSave(uint8_t) {}

  uint8_t *getBufferPtr() { return buf_; }
  uint8_t getBufferTileWidth() const { return 16; }
  uint8_t getBufferTileHeight() const { return 8; }
  void clearBuffer() { memset(buf_, 0, sizeof(buf_)); }

  void sendBuffer() { updateDisplayArea(0, 0, 16, 8); }

  // Same contract as the real one: only these tiles reach the panel.
  void updateDisplayArea(uint8_t tx, uint8_t ty, uint8_t tw, uint8_t th) {
    for (int y = ty; y < ty + th && y < 8; y++) {
      for (int x = tx * 8; x < (tx + tw) * 8 && x < 128; x++) emuHw.panel[y * 128 + x] = buf_[y * 128 + x];
      // per page: address + 3 commands + control byte + the pixels
      emuHw.i2cBytes += 6 + (uint32_t)tw * 8;
    }
    emuHw.panelUpdates++;
  }

 private:
  uint8_t buf_[1024];
};

typedef U8G2_SSD1306_128X64_NONAME_F_HW_I2C U8G2;

#endif
