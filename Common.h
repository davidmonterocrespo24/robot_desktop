#ifndef COMMON_h
#define COMMON_h
#include <ESP32Servo.h>
#include <U8g2lib.h>
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

#define DHTPIN 15     // Pin de datos del sensor DHT11
#define DHTTYPE DHT11 // Tipo de sensor DHT (puedes cambiar a DHT22 si es necesario)

#define  SERVO1_Derecha  12
#define  SERVO2_Izquierda 13
#define  PIR_PIN  4
#define  SOUND_PIN  2
#define  LDR_PIN  34


 enum RobotState
{
  INICIO,
  NORMAL,
  SOUND,
  MOVE,
  SLEEP,
  WEATHER
};

extern TaskHandle_t faceHandle;
extern Servo ServoDerecha;
extern Servo ServoIzquierda;

extern  TaskHandle_t servoHandle;



extern  TaskHandle_t soundHandle;

extern  TaskHandle_t sensorHandle;

#define speakerPin 2

#define MAX_STATES 100 // Número máximo de estados en la cola


extern  RobotState stateQueue[MAX_STATES]; // Cola de estados
//extern  int queueFront = 0;                // Frente de la cola
//extern  int queueRear = -1;                // Final de la cola
//extern  int queueItemCount = 0;            // Número de elementos en la cola

extern  unsigned long lastMovementTime;         // Tiempo del último movimiento detectado
extern  const unsigned long WEATHER_TIMEOUT; // 10 minutos en milisegundos
extern  const unsigned long SLEEP_TIMEOUT ;  // 1 hora en milisegundos

extern  unsigned long lastTriggerTime;
extern  bool lastStateMovement;
extern  bool movementDetected;

// Flags para controlar el estado
extern  bool weatherTimeoutReached;
extern  bool sleepTimeoutReached ;

// Función para insertar un estado en la cola
extern  RobotState lastInsertedState; // Variable para almacenar el último estado insertado
// Estado actual
extern  RobotState currentState; // Variable para almacenar el último estado insertado

extern bool isLightOn();
extern bool isMovementDetected();


#endif