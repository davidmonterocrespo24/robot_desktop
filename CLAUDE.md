# CLAUDE.md

Desktop robot: ESP32 + SSD1306 OLED face + two servo ears + sensors. Code and comments
in English, README in Spanish.

## Build & run

**Emulator (Windows, use this for all development):**
```bash
cd emulator/
taskkill /F /IM robot_emu.exe 2>nul
mingw32-make            # robot_emu.exe + facetool.exe
./robot_emu.exe
```

**Look at a change without a human:**
```bash
./facetool.exe sheet out.gif          # every expression on one page
./facetool.exe gif Love love.gif      # one transition
./facetool.exe strip Pacman out.gif   # 12 moments of an act on one page
./robot_emu.exe --script "2:expr=Love; 4:shot=C:/abs/path/a.png; 4:quit"
```
Screenshot paths must be absolute (the emulator changes to its own folder).

**Hardware:** `arduino-cli compile --fqbn esp32:esp32:esp32 .` (core 2.0.17, U8g2,
ESP32Servo, DHT sensor library).

## Architecture

`esp32-eyes.ino` is one loop at 30 fps: `Sensors -> Brain -> FaceEngine -> Canvas -> OLED`,
plus `Ears` and `Buzzer`. No FreeRTOS tasks.

- `src/core`, `src/face`, `src/clips` are pure C++ (no Arduino headers, time is a
  parameter). Keep them that way: `facetool` and the emulator depend on it.
- `src/body`, `src/brain` use Arduino APIs; the emulator provides them through the mocks
  in `emulator/` (`Arduino.h`, `U8g2lib.h`, `ESP32Servo.h`, `DHT.h`), which read and
  write `EmuHw`.
- `emulator/sketch_wrapper.cpp` includes the `.ino` (same trick as car_game).
- `legacy/` is the previous engine, kept for reference. Arduino does not compile it.
- `doc/circuito.html` (and `circuito.png`, `circuito-alimentacion.png`) is the wiring
  and assembly guide. It follows the pins of `Config.h`: update it when they change.
- The state machine of `src/brain/Brain.cpp` is drawn in two places that must follow
  the code: `doc/maquina-de-estados.html` (and the two PNGs made from its figures) and
  `emulator/StateView.cpp` (live, key Q). Every change of state goes through
  `Brain::enter(state, why)`; the `why` text is what the emulator shows.

## Rules of the face engine

- The table in `Expressions.cpp` must follow the order of `enum ExprId`.
- Acts (`Acts*.cpp`) are pure functions of time: no static state, randomness only
  through `hash01()`. One-shot events use `f.crossed(t)`. New act = two functions +
  `ACT_DECL` in `ActList.h` + one row in `Acts.cpp` + the `.o` in `emulator/Makefile`.
- Props behind the eyes are drawn on `ACT_BACK` with `f.halo = true`; props in front use
  a dark rim (`drawBall`, or a thicker `C_OFF` stroke first) so they read over a lit eye.
- Every expression needs at least one edge in `ExpressionGraph.cpp`.
- Float literals need the `f` suffix: the ESP32 FPU is single precision only.
- `src/clips/ClipData.cpp` is generated (`tools/gif2clip.py`), never edit it.
- `emulator/assets/robot/` is generated (`tools/export_cad.py`, needs FreeCAD).
- `cad/stl/`, `cad/ensamblado/`, `cad/esp32_eyes.step` and `cad/export_stl.log` are generated
  by `cad/export_stl.py` (FreeCAD) from `doc/Robot ESP32 V2 - copia.FCStd`. The ears are
  not in the FCStd: they are defined in that script. The README pictures `doc/cad-*.png`
  come from `tools/render_stl.py`.
