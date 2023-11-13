/***************************************************
 * ESP32-Eyes, Copyright (c) 2023 Alastair Aitchison, Playful Technology
 * Based on previous work by Luis Llamas (www.luisllamas.es)
 *
 ****************************************************/

#include <Wire.h>
#include <ESP32Servo.h>
#include "Face.h"
// CONSTANTS
const byte joystickPins[] = {25, 26};
const byte blinkPin = 16;
const byte moodPins[] = {12, 13, 15};

TaskHandle_t faceHandle;
Servo ServoDerecha;
Servo ServoIzquierda;
#define SERVO1_Derecha 12
#define SERVO2_Izquierda 13
TaskHandle_t servoHandle;
// GLOBALS
Face *face;
eEmotions previousEmotion ;

void vUpdateFaceTask(void *pvParameters)
{
  while (1)
  {
    // Actualizar la cara
    // Serial.println("Actualizar la cara");
    face->Update();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void moveServoWithPID(Servo servo, int fromAngle, int toAngle)
{
  servo.write(fromAngle);
  vTaskDelay(pdMS_TO_TICKS(105));
  servo.write(toAngle);
}

void vMoveServoTask(void *pvParameters)
{
  while (1)
  {
    Serial.println(face->Behavior.CurrentEmotion);   
   
      Serial.println("vMoveServoTask ............");
      Serial.println("Cambio Estado........");
      Serial.println(face->Behavior.CurrentEmotion);

      switch (face->Behavior.CurrentEmotion)
      {
      case eEmotions::Normal:
        Serial.println("Normal Motor");
        ServoDerecha.write(0);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoDerecha.write(170);
        ServoIzquierda.write(170);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoIzquierda.write(0);
        break;

      case eEmotions::Angry:
        Serial.println("Angry Motor");
        ServoDerecha.write(0);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoDerecha.write(90);
        ServoIzquierda.write(90);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoIzquierda.write(0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ServoDerecha.write(90);
        ServoIzquierda.write(0);
        break;

      case eEmotions::Happy:
        Serial.println("Happy Motor");
        ServoDerecha.write(0);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoDerecha.write(45);
        ServoIzquierda.write(45);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoIzquierda.write(0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ServoDerecha.write(45);
        ServoIzquierda.write(170);
        break;

      case eEmotions::Surprised:
        Serial.println("Surprised Motor");
        ServoDerecha.write(0);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoDerecha.write(60);
        ServoIzquierda.write(60);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoIzquierda.write(0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ServoDerecha.write(60);
        ServoIzquierda.write(0);
        break;

      case eEmotions::Sad:
        Serial.println("Sad Motor");
        ServoDerecha.write(0);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoDerecha.write(90);
        ServoIzquierda.write(90);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoIzquierda.write(0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ServoDerecha.write(90);
        ServoIzquierda.write(0);
        break;

      case eEmotions::Worried:
        Serial.println("Worried Motor");
        ServoDerecha.write(0);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoDerecha.write(90);
        ServoIzquierda.write(90);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoIzquierda.write(0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ServoDerecha.write(90);
        ServoIzquierda.write(0);
        break;

      case eEmotions::Focused:
        Serial.println("Focused Motor");
        ServoDerecha.write(0);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoDerecha.write(90);
        ServoIzquierda.write(90);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoIzquierda.write(0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ServoDerecha.write(90);
        ServoIzquierda.write(0);
        break;

      case eEmotions::Annoyed:
        Serial.println("Annoyed Motor");
        ServoDerecha.write(0);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoDerecha.write(90);
        ServoIzquierda.write(90);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoIzquierda.write(0);
        delay(1000);
        ServoDerecha.write(90);
        ServoIzquierda.write(0);
        break;

      case eEmotions::Skeptic:
        Serial.println("Skeptic Motor");
        ServoDerecha.write(0);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoDerecha.write(90);
        ServoIzquierda.write(90);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoIzquierda.write(0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ServoDerecha.write(90);
        ServoIzquierda.write(0);
        break;

      case eEmotions::Frustrated:
        Serial.println("Frustrated Motor");
        ServoDerecha.write(0);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoDerecha.write(90);
        ServoIzquierda.write(90);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoIzquierda.write(0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ServoDerecha.write(90);
        ServoIzquierda.write(0);
        break;

      case eEmotions::Unimpressed:
        Serial.println("Unimpressed Motor");
        ServoDerecha.write(0);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoDerecha.write(90);
        ServoIzquierda.write(90);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoIzquierda.write(0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ServoDerecha.write(90);
        ServoIzquierda.write(0);
        break;

      case eEmotions::Sleepy:
        Serial.println("Sleepy Motor");
        ServoDerecha.write(0);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoDerecha.write(45);
        ServoIzquierda.write(45);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoIzquierda.write(0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ServoDerecha.write(45);
        ServoIzquierda.write(170);
        break;

      case eEmotions::Suspicious:
        Serial.println("Suspicious Motor");
        ServoDerecha.write(0);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoDerecha.write(60);
        ServoIzquierda.write(60);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoIzquierda.write(0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ServoDerecha.write(60);
        ServoIzquierda.write(0);
        break;

      case eEmotions::Squint:
        Serial.println("Squint Motor");
        ServoDerecha.write(0);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoDerecha.write(90);
        ServoIzquierda.write(90);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoIzquierda.write(0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ServoDerecha.write(90);
        ServoIzquierda.write(0);
        break;

      case eEmotions::Furious:
        Serial.println("Furious Motor");
        ServoDerecha.write(0);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoDerecha.write(45);
        ServoIzquierda.write(45);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoIzquierda.write(0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ServoDerecha.write(45);
        ServoIzquierda.write(170);
        break;

      case eEmotions::Scared:
        Serial.println("Scared Motor");
        ServoDerecha.write(0);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoDerecha.write(60);
        ServoIzquierda.write(60);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoIzquierda.write(0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ServoDerecha.write(60);
        ServoIzquierda.write(0);
        break;

      default:
        break;
      }
   

    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

void setup(void)
{
  // Create a serial connection
  Serial.begin(115200);
  Serial.println(__FILE__ __DATE__);

  // Configure input pins
  pinMode(blinkPin, INPUT_PULLUP);

  // Create a new face
  face = new Face(/* screenWidth = */ 128, /* screenHeight = */ 64, /* eyeSize = */ 40);
  // Assign the current expression
  face->Expression.GoTo_Sleepy();
  previousEmotion = face->Behavior.CurrentEmotion;

  // Assign a weight to each emotion
  /*face->Behavior.SetEmotion(eEmotions::Normal, 1.0);
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
  face->RandomBehavior = true;*/

  // Automatically blink
  face->RandomBlink = false;
  // Set blink rate
  face->Blink.Timer.SetIntervalMillis(8000);
  face->Behavior.Timer.SetIntervalMillis(15000);

  // Automatically choose a new random direction to look
  face->RandomLook = true;
  ServoDerecha.attach(SERVO1_Derecha);
  ServoIzquierda.attach(SERVO2_Izquierda);
  ServoIzquierda.write(0);
  ServoDerecha.write(170);
  xTaskCreatePinnedToCore(
      vUpdateFaceTask, /* Task function. */
      "Update face",   /* name of task. */
      10000,           /* Stack size of task */
      NULL,            /* parameter of the task */
      10,              /* priority of the task */
      &faceHandle,     /* Task handle to keep track of created task */
      0);              /* pin task to core 0 */
  xTaskCreatePinnedToCore(
      vMoveServoTask, /* Task function. */
      "Update Servo", /* name of task. */
      10000,          /* Stack size of task */
      NULL,           /* parameter of the task */
      10,             /* priority of the task */
      &servoHandle,   /* Task handle to keep track of created task */
      1);             /* pin task to core 0 */
}

void loop()
{
  // vUpdateFaceTask();
}