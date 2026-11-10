#ifndef _SENSORDRIVER_h
#define _SENSORDRIVER_h
#include "Common.h"

int minLuz = 10;   // Valor inicial negativo para indicar que no se ha leído aún
int maxLuz = 10;
int umbralLuz = 0;
unsigned long tiempoUltimaLectura = 0;
const unsigned long intervaloAjuste = 10000; // 20 segundos recalcula el umbral

void ajustarUmbral() {
  umbralLuz = (minLuz + maxLuz) / 2;
  Serial.print("Nuevo umbral de luz calculado: ");
  Serial.println(umbralLuz);
}

bool isLightOn() {
  int luz = analogRead(LDR_PIN);
  if (luz < minLuz) minLuz = luz;
  if (luz > maxLuz) maxLuz = luz;
  ajustarUmbral();      
  bool hayLuz = (luz < umbralLuz);  
  if (!hayLuz) {
    Serial.print("No hay Luz: " );
     Serial.println(luz);
  } else {
    Serial.print("Hay Luz: ");
     Serial.println(luz);
  }
  return hayLuz;
}

bool isMovementDetected()
{
  int currentStateMovement = digitalRead(PIR_PIN);

  if (currentStateMovement != lastStateMovement)
  {
    lastTriggerTime = millis();               // Actualiza el tiempo del último cambio de estado
    lastStateMovement = currentStateMovement; // Actualiza el último estado registrado
    movementDetected = true;                  // Indica que se ha detectado un cambio de estado
  }

  // Verifica si ha pasado menos de 5 segundos desde el último cambio de estado
  if (movementDetected && millis() - lastTriggerTime < 3000)
  {
    Serial.println("Movimiento DETECTADO..... 130");
    return true; // Retorna true si se detectó un cambio de estado en menos de 5 segundos
  }

  // Verifica si han pasado más de 5 segundos desde el último cambio de estado
  if (millis() - lastTriggerTime >= 3000)
  {
    movementDetected = false; // Reinicia la bandera de detección de cambio de estado
  }

  return false; // Si no se cumple ninguna condición, retorna false
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
  Serial.println("INICIO DE LA LECTURA DE SENSORES");
  int lightSignalCount = 0;
  unsigned long lastLightSignalTime = 0;
  bool isLightOnPreviously = false;

  while (1)
  {
    int STATE = currentState;

    if (isMovementDetected() && STATE == SLEEP)
    {
      // insertState(NORMAL);
      currentState = NORMAL;
      Serial.println(">>>>>>>>>>>>>>>>>>>>>>MOVIMIENTO DETECTADO AND STATE=SLEEP  171");
      lastMovementTime = millis(); // Actualizar el tiempo del último movimiento detectado
      sleepTimeoutReached = false; // Reiniciar la bandera del timeout de sueño
      weatherTimeoutReached = false;
    }
    else if (isMovementDetected() && STATE == NORMAL)
    {
      // insertState(MOVE);
      currentState = MOVE;
      Serial.println(">>>>>>>>>>>>>>>>>>>>>>>MOVIMIENTO DETECTADO AND STATE == NORMAL 179");
      lastMovementTime = millis(); // Actualizar el tiempo del último movimiento detectado
      sleepTimeoutReached = false; // Reiniciar la bandera del timeout de sueño
      weatherTimeoutReached = false;
    }
    else if (isMovementDetected() && STATE == WEATHER)
    {
      Serial.println(">>>>>>>>>>>>>>>>>>>>>>>>> MOVIMIENTO DETECTADO AND STATE == WEATHER 93");
      lastMovementTime = millis(); // Actualizar el tiempo del último movimiento detectado
      sleepTimeoutReached = false; // Reiniciar la bandera del timeout de sueño
      weatherTimeoutReached = false;
      currentState = NORMAL;
    }
    // Lógica adicional para cambiar al estado WEATHER después de 10 minutos sin movimiento
    if (millis() - lastMovementTime >= WEATHER_TIMEOUT && !weatherTimeoutReached)
    {
      // insertState(WEATHER);
      currentState = WEATHER;
      Serial.println(">>>>>>>>>>>>>>>>>>>>>>WEATHER después 10 minutos sin movimiento");
      weatherTimeoutReached = true; // Marcar que se alcanzó el tiempo límite para el estado WEATHER
    }

    // Lógica adicional para cambiar al estado SLEEP después de una hora sin movimiento
    if (millis() - lastMovementTime >= SLEEP_TIMEOUT && !sleepTimeoutReached)
    {
      // insertState(SLEEP);
      currentState = SLEEP;
      Serial.println(">>>>>>>>>>>>>>>>>>>>>SLEEP después de una hora sin movimiento");
      sleepTimeoutReached = true; // Marcar que se alcanzó el tiempo límite para el estado SLEEP
    }

    if (STATE == MOVE && millis() - lastMovementTime >= 30000)
    {
      currentState = NORMAL;
      Serial.println(">>>>>>>>>>>>>>>>>>>>>VOLVIENDO A NORMAL después de 30 segundos en MOVE");
    }

    // Lógica adicional para cambiar al estado WEATHER si se recibe y deja de recibir señal de luz 3 veces
    if (!isLightOn())
    {
      // insertState(SLEEP);
      currentState = SLEEP;
      Serial.println("NO HAY LUZ --> SLEEP   160");
    }

    if (isLightOn())
    {
      if (!isLightOnPreviously)
      {
        lightSignalCount++;
        isLightOnPreviously = true;
        Serial.println("Señal de luz recibida");
      }
    }
    else
    {
      isLightOnPreviously = false;
    }

    if (lightSignalCount >= 3)
    {
      currentState = WEATHER;
      Serial.println(">>>>>>>>>>>>>>>>>>>>> Cambiando a WEATHER después de recibir y dejar de recibir señal de luz 3 veces");
      lightSignalCount = 0; // Reiniciar el contador
    }

    vTaskDelay(pdMS_TO_TICKS(500)); // Espera antes de volver a verificar los sensores
  }
}

#endif