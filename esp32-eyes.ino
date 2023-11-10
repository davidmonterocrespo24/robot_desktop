/***************************************************
 * ESP32-Eyes, Copyright (c) 2023 Alastair Aitchison, Playful Technology
 * Based on previous work by Luis Llamas (www.luisllamas.es)
 *
 ****************************************************/

// INCLUDES
// Built-in Arduino I2C library
#include <Wire.h>
// Defines all face functionality
#include "Face.h"

// CONSTANTS
const byte joystickPins[] = {25, 26};
const byte blinkPin = 16;
const byte moodPins[] = {12, 13, 15};
const eEmotions moods[] = {eEmotions::Angry, eEmotions::Sad, eEmotions::Surprised}; 

// GLOBALS
Face *face;

void setup(void) {
  // Create a serial connection
  Serial.begin(115200);
  Serial.println(__FILE__ __DATE__);

  // Configure input pins
  pinMode(blinkPin, INPUT_PULLUP);

  // Create a new face
  face = new Face(/* screenWidth = */ 128, /* screenHeight = */ 64, /* eyeSize = */ 40);
  // Assign the current expression
  face->Expression.GoTo_Normal();

  // Assign a weight to each emotion
    face->Behavior.SetEmotion(eEmotions::Normal, 1.0);
  face->Behavior.SetEmotion(eEmotions::Angry, 1.0);
  face->Behavior.SetEmotion(eEmotions::Glee, 1.0);
  face->Behavior.SetEmotion(eEmotions::Happy, 1.0);
  face->Behavior.SetEmotion(eEmotions::Sad, 1.0);
  face->Behavior.SetEmotion(eEmotions::Worried, 1.0);
  face->Behavior.SetEmotion(eEmotions::Focused, 1.0);
  face->Behavior.SetEmotion(eEmotions::Annoyed, 1.0);
  face->Behavior.SetEmotion(eEmotions::Surprised, 1.0);
  face->Behavior.SetEmotion(eEmotions::Skeptic, 1.0);
  face->Behavior.SetEmotion(eEmotions::Frustrated, 1.0);
  face->Behavior.SetEmotion(eEmotions::Unimpressed, 1.0);
  face->Behavior.SetEmotion(eEmotions::Sleepy, 1.0);
  face->Behavior.SetEmotion(eEmotions::Suspicious, 1.0);
  face->Behavior.SetEmotion(eEmotions::Squint, 1.0);
  face->Behavior.SetEmotion(eEmotions::Furious, 1.0);
  face->Behavior.SetEmotion(eEmotions::Scared, 1.0);
  face->Behavior.SetEmotion(eEmotions::Awe, 1.0);
  face->RandomBehavior = true;

  // Automatically blink
  face->RandomBlink = true;
  // Set blink rate
  face->Blink.Timer.SetIntervalMillis(8000);
  face->Behavior.Timer.SetIntervalMillis(15000);


  // Automatically choose a new random direction to look
  face->RandomLook = true;
}


void loop(){
  face->Update();
}