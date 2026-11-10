#include <U8g2lib.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>

#include "Face.h"
#include "Weather.h"
#include "Common.h"
#include "SensorDriver.h"
#include "ServoMotorDriver.h"
// GLOBALS
Face *face;

unsigned long lastMovementTime = 0;           // Tiempo del último movimiento detectado
const unsigned long WEATHER_TIMEOUT = 600000; // 10 minutos en milisegundos
const unsigned long SLEEP_TIMEOUT = 3600000;  // 1 hora en milisegundos

int progress = 0;

// Flags para controlar el estado
bool weatherTimeoutReached = false;
bool sleepTimeoutReached = false;

unsigned long lastTriggerTime = 0;
bool lastStateMovement = false;
bool movementDetected = false;

RobotState lastInsertedState = SLEEP;
// Estado actual
RobotState currentState = SLEEP;

TaskHandle_t faceHandle;
Servo ServoDerecha;
Servo ServoIzquierda;

TaskHandle_t servoHandle;
// GLOBALS

eEmotions previousEmotion;

TaskHandle_t soundHandle;

TaskHandle_t sensorHandle;

void vUpdateFaceTask(void *pvParameters)
{
  while (1)
  {

    face->RandomBehavior = false;
    face->RandomBlink = true;
    face->RandomLook = true;

    if (currentState == NORMAL)
    {
      Serial.println("STATE Normal   224****************************************");
      face->RandomBehavior = true;
    }
    else if (currentState == SOUND)
    {
      // Acciones para el estado NERVIOSO
      eEmotions cEmotion = face->Behavior.CurrentEmotion;
      if (cEmotion != eEmotions::Surprised)
      {
        face->Expression.GoTo_Surprised();
      }
      Serial.println("STATE = SONIDO ..... *************************************************");
    }
    else if (currentState == MOVE)
    {
      // Acciones para el estado MOVE
      Serial.println("STATE = MOVE ......  *************************************************");
      eEmotions cEmotion = face->Behavior.CurrentEmotion;
      if (cEmotion != eEmotions::Focused)
      {
        face->Expression.GoTo_Focused();
      }
    }
    else if (currentState == SLEEP)
    {
      Serial.println("STATE = SLEEP ..... **************************************************");
      face->RandomBehavior = false;
      face->RandomBlink = false;
      face->RandomLook = false;
      Serial.println(face->Behavior.CurrentEmotion);
      Serial.println(eEmotions::Sleepy);
      if (face->Behavior.CurrentEmotion != eEmotions::Sleepy)
      {
        face->Behavior.CurrentEmotion = eEmotions::Sleepy;
        face->Expression.GoTo_Sleepy();
        Serial.println("GoTo_Sleepy");
      }
      else
      {
        Serial.println("Distinto ******------------------------------------------------------------------------------************");
        face->Update();
      }
    }
    else if (currentState == WEATHER)
    {

      Serial.println("STATE = Weather ..... ************************************************");
      Serial.println(analogRead(LDR_PIN));
      face->Expression.GoTo_Normal();
      updateWEATHER();
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    else if (currentState == INICIO)
    {
      currentState = SLEEP;
    }

    face->Update();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void setup(void)
{
  // Create a serial connection
  Serial.begin(115200);
  Serial.println("Iniciando................");
  ServoDerecha.write(0);
  ServoDerecha.write(90);

  pinMode(PIR_PIN, INPUT);
  // pinMode(SOUND_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  // insertState(INICIO);
  currentState = SLEEP;
  lastStateMovement = digitalRead(PIR_PIN);
  // Configure input pins
  pinMode(SOUND_PIN, OUTPUT);
  randomSeed(analogRead(0));

  // StarWarsPlay();
  // Create a new face

  face = new Face(/* screenWidth = */ 128, /* screenHeight = */ 64, /* eyeSize = */ 40);

  initDht();

  delay(3000);
  // Assign the current expression
  face->Expression.GoTo_Sleepy();
  Serial.println("GoTo_Sleepy");

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

  face->RandomBehavior = false;

  // Automatically blink
  face->RandomBlink = true;
  // Set blink rate
  face->Blink.Timer.SetIntervalMillis(8000);
  face->Behavior.Timer.SetIntervalMillis(15000);

  // Automatically choose a new random direction to look
  face->RandomLook = true;
  ServoDerecha.attach(SERVO1_Derecha);
  ServoIzquierda.attach(SERVO2_Izquierda);
  ServoIzquierda.write(0);
  ServoDerecha.write(170);

  delay(3000);

  xTaskCreatePinnedToCore(
      vUpdateFaceTask, /* Task function. */
      "Update face",   /* name of task. */
      10000,           /* Stack size of task */
      NULL,            /* parameter of the task */
      1,               /* priority of the task */
      &faceHandle,     /* Task handle to keep track of created task */
      0);              /* pin task to core 0 */

  xTaskCreatePinnedToCore(
      vMoveServoTask, /* Task function. */
      "Update Servo", /* name of task. */
      10000,          /* Stack size of task */
      face,           /* parameter of the task */
      5,              /* priority of the task */
      &servoHandle,   /* Task handle to keep track of created task */
      1);             /* pin task to core 0 */

  xTaskCreatePinnedToCore(
      vReadSensorTask, /* Task function. */
      "Read Sensor",   /* name of task. */
      10000,           /* Stack size of task */
      NULL,            /* parameter of the task */
      10,              /* priority of the task */
      &sensorHandle,   /* Task handle to keep track of created task */
      1);              /* pin task to core 0 */

  /* xTaskCreatePinnedToCore(
   vSoundTask, /* Task function. */
  //  "Sound",   /* name of task. */
  //  10000,           /* Stack size of task */
  // NULL,            /* parameter of the task */
  //  10,              /* priority of the task */
  //  &sensorHandle,   /* Task handle to keep track of created task */
  //  1);              /* pin task to core 0 */

  Serial.println("Finalizando MAIN");
}

void loop()
{
  // vUpdateFaceTask();
}
