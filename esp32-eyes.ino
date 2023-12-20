#include <U8g2lib.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <ESP32Servo.h>
#include "Face.h"
#include "sound.h"
#include "Weather.h"


#define DHTPIN 15     // Pin de datos del sensor DHT11
#define DHTTYPE DHT11 // Tipo de sensor DHT (puedes cambiar a DHT22 si es necesario)

const int SERVO1_Derecha = 12;
const int SERVO2_Izquierda = 13;
const int PIR_PIN = 4;
const int SOUND_PIN = 2;
const int LDR_PIN = 34;


TaskHandle_t faceHandle;
Servo ServoDerecha;
Servo ServoIzquierda;

TaskHandle_t servoHandle;
// GLOBALS
Face *face;

eEmotions previousEmotion;

TaskHandle_t soundHandle;

TaskHandle_t sensorHandle;

#define speakerPin 2

#define MAX_STATES 100 // Número máximo de estados en la cola

enum RobotState
{
  INICIO,
  NORMAL,
  SOUND,
  MOVE,
  SLEEP,
  WEATHER
};

RobotState stateQueue[MAX_STATES]; // Cola de estados
int queueFront = 0;                // Frente de la cola
int queueRear = -1;                // Final de la cola
int queueItemCount = 0;            // Número de elementos en la cola

unsigned long lastMovementTime = 0;           // Tiempo del último movimiento detectado
const unsigned long WEATHER_TIMEOUT = 600000; // 10 minutos en milisegundos
const unsigned long SLEEP_TIMEOUT = 3600000;  // 1 hora en milisegundos

int progress = 0;

// Flags para controlar el estado
bool weatherTimeoutReached = false;
bool sleepTimeoutReached = false;

// Función para insertar un estado en la cola
RobotState lastInsertedState = WEATHER; // Variable para almacenar el último estado insertado

// Función para insertar un estado en la cola
void insertState(RobotState state)
{
  if (queueItemCount < MAX_STATES && state != lastInsertedState)
  {
    if (queueRear == MAX_STATES - 1)
    {
      queueRear = -1;
    }
    stateQueue[++queueRear] = state;
    queueItemCount++;
    lastInsertedState = state; // Actualiza el último estado insertado
  }
}

// Función para obtener y eliminar un estado de la cola
RobotState removeState()
{
  RobotState state = stateQueue[queueFront++];
  if (queueFront == MAX_STATES)
  {
    queueFront = 0;
  }
  queueItemCount--;
  return state;
}

RobotState getFirstState()
{
  return stateQueue[queueFront];
}

bool isLightOn()
{
  int luz = analogRead(LDR_PIN);
  bool hayLuz = (luz < 2000); // Ajusta el valor según tu entorno de luz
  if (!hayLuz)
  {
    Serial.println("No hay Luz" + String(luz));
  }

  return hayLuz;
}
bool isMovementDetected()
{
  int move = digitalRead(PIR_PIN);
  if (move == LOW)
  {
    Serial.println("Movimiento Detectado");
    return true;
  }
  else
  { 
    return false;
  }
}
bool isSoundDetected()
{
  int sonido = digitalRead(SOUND_PIN);
  if (sonido == HIGH)
  {
    Serial.println("Sonido Detectado");
    return true;
  }
  else
  {
    return false;
  }
}

void vReadSensorTask(void *pvParameters)
{
  while (1)
  {
    int STATE = getFirstState();
    if (!isLightOn())
    {
      insertState(SLEEP);
      Serial.println("Insert SLEEP");
    }
   /* else if (isSoundDetected())
    {
      insertState(SOUND);
      Serial.println("Insert SOUND");
    }*/
    else if (isMovementDetected() && STATE == SLEEP)
    {
      insertState(NORMAL);
      Serial.println("Insert NORMAL isMovementDetected");
      lastMovementTime = millis(); // Actualizar el tiempo del último movimiento detectado
      sleepTimeoutReached = false; // Reiniciar la bandera del timeout de sueño
    }
    else if (isMovementDetected() && STATE != SLEEP)
    {
      insertState(MOVE);
      Serial.println("Insert MOVE isMovementDetected");
      lastMovementTime = millis(); // Actualizar el tiempo del último movimiento detectado
      sleepTimeoutReached = false; // Reiniciar la bandera del timeout de sueño
    }
    // Lógica adicional para cambiar al estado WEATHER después de 10 minutos sin movimiento
    if (millis() - lastMovementTime >= WEATHER_TIMEOUT && !weatherTimeoutReached)
    {
      insertState(WEATHER);
      Serial.println("Insert WEATHER después 10 minutos sin movimiento");
      weatherTimeoutReached = true; // Marcar que se alcanzó el tiempo límite para el estado WEATHER
    }

    // Lógica adicional para cambiar al estado SLEEP después de una hora sin movimiento
    if (millis() - lastMovementTime >= SLEEP_TIMEOUT && !sleepTimeoutReached)
    {
      insertState(SLEEP);
      Serial.println("Insert SLEEP después de una hora sin movimiento");
      sleepTimeoutReached = true; // Marcar que se alcanzó el tiempo límite para el estado SLEEP
    }

    vTaskDelay(pdMS_TO_TICKS(500)); // Espera antes de volver a verificar los sensores
  }
}

void vUpdateFaceTask(void *pvParameters)
{
  while (1)
  {
    unsigned long lastStateChangeTime = millis();
    while (queueItemCount > 0)
    {
      unsigned long currentTime = millis();
      // Verifica si han pasado al menos 30 segundos desde el último cambio de estado
      if (currentTime - lastStateChangeTime >= 5000)
      {
        RobotState currentState = removeState();
        Serial.println("Estado........");
        Serial.println(currentState);
        face->RandomBehavior = false;
        eEmotions pEmotion = face->Behavior.CurrentEmotion;

        if (currentState == NORMAL)
        {
          Serial.println("Normal");
          if (pEmotion != eEmotions::Normal)
          {
            Serial.println("RandomBehavior ");
            face->RandomBehavior = true;
           // playerTone();
          }
        }
        else if (currentState == SOUND)
        {
          // Acciones para el estado NERVIOSO
          Serial.println("Sound");
          face->Expression.GoTo_Surprised();
        }
        else if (currentState == MOVE)
        {
          // Acciones para el estado MOVE
          Serial.println("*************Move************");
          face->Expression.GoTo_Annoyed();
        }
        else if (currentState == SLEEP)
        {
          Serial.println("Sleep");
         // face->Expression.GoTo_Sleepy();
         updateWEATHER();
        }
        else if (currentState == WEATHER)
        {
          Serial.println("Weather");
          updateWEATHER();
        }
        else
        {
          Serial.println("Default");
          insertState(NORMAL);
        }

        lastStateChangeTime = currentTime;
        Serial.println("DEFAULT face->Update() ");
        face->Update();
      }
      else if (getFirstState() == INICIO)
      {
        removeState();
        insertState(SLEEP);
        lastStateChangeTime = currentTime;
        Serial.println("INICIO ......... ESTATE face->Update() ");
      }

      face->Update();

      vTaskDelay(pdMS_TO_TICKS(100));
    }
    //Serial.println("No hay estados en la cola");
    face->RandomBehavior = true;
    face->Update();
   // Serial.println("ESTATE face->Update() ");
    insertState(NORMAL);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
void moveServoWithPID(Servo servo, int fromAngle, int toAngle)
{
  servo.write(fromAngle);
  vTaskDelay(pdMS_TO_TICKS(105));
  servo.write(toAngle);
}

void vSoundTask(void *pvParameters)
{
  playerTone();
  vTaskDelay(pdMS_TO_TICKS(10000));
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
  Serial.println("Iniciando");
  

  pinMode(PIR_PIN, INPUT);
  //pinMode(SOUND_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  insertState(INICIO);
  // Configure input pins
  pinMode(SOUND_PIN, OUTPUT);
  randomSeed(analogRead(0));
 // StarWars();
 // StarWarsPlay();
  // Create a new face
  
  face = new Face(/* screenWidth = */ 128, /* screenHeight = */ 64, /* eyeSize = */ 40);
  
  playerTone();
  initDht();
  // Assign the current expression
  face->Expression.GoTo_Sleepy();
  Serial.println("GoTo_Sleepy");
  previousEmotion = face->Behavior.CurrentEmotion;

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
      NULL,           /* parameter of the task */
    5,             /* priority of the task */
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

  Serial.println("Finalizando");
}

void loop()
{
  // vUpdateFaceTask();
}