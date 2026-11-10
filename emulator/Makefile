# Windows emulator of the robot (same idea as car_game/emulator).
#
#   make            builds robot_emu.exe (window with the 3D robot) and facetool.exe
#   make run        builds and starts the emulator
#   make gifs       writes every expression and act as a GIF into ../doc
#   make clean
#
# Needs g++ (w64devkit) and raylib. Change RAYLIB_PATH if yours is elsewhere.
RAYLIB_PATH ?= C:/raylib

CXX = g++
# The mocks in this folder stand in for <Arduino.h>, <U8g2lib.h>, <ESP32Servo.h>
# and <DHT.h>, so "-I." must come first.
#
# The emulator wires a virtual sound sensor to GPIO35 so the reactions to
# noise can be tried. The real robot only has it if you connect one and set
# PIN_MIC in Config.h.
CXXFLAGS = -I. -I$(RAYLIB_PATH)/include -O2 -std=c++17 -Wall -Wextra -Wno-missing-field-initializers -MMD -MP \
           -DEMULATOR -DPIN_MIC=35 -DEMU_VIRTUAL_MIC
LDFLAGS = -L$(RAYLIB_PATH)/lib -lraylib -lopengl32 -lgdi32 -lwinmm -static-libgcc -static-libstdc++

VPATH = ../src/core ../src/face ../src/clips ../src/body ../src/brain

ENGINE = Canvas.o FaceRenderer.o Decor.o Expressions.o ExpressionGraph.o FaceEngine.o \
         Acts.o ActsMotion.o ActsProps.o ActsGames.o ActsLife.o Clip.o ClipData.o
ROBOT = Ears.o Buzzer.o Sensors.o Brain.o InfoScreen.o sketch_wrapper.o Arduino.o
EMU_OBJS = main.o Scene.o StateView.o $(ROBOT) $(ENGINE)
TOOL_OBJS = facetool.o $(ENGINE)

all: robot_emu.exe facetool.exe

robot_emu.exe: $(EMU_OBJS)
	$(CXX) $(EMU_OBJS) -o $@ $(LDFLAGS)

facetool.exe: $(TOOL_OBJS)
	$(CXX) $(TOOL_OBJS) -o $@ -static-libgcc -static-libstdc++

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# the sketch is included by the wrapper
sketch_wrapper.o: ../esp32-eyes.ino ../Config.h

run: robot_emu.exe
	./robot_emu.exe

gifs: facetool.exe
	./facetool.exe all ../doc/expressions
	./facetool.exe acts ../doc/animations

clean:
	-rm -f $(EMU_OBJS) $(TOOL_OBJS) $(EMU_OBJS:.o=.d) $(TOOL_OBJS:.o=.d) robot_emu.exe facetool.exe

.PHONY: all run gifs clean

-include $(EMU_OBJS:.o=.d) $(TOOL_OBJS:.o=.d)
