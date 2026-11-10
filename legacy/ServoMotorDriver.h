
#ifndef _SERVOMOTORDRIVER_h
#define _SERVOMOTORDRIVER_h
#include "Common.h"

#include "Face.h"

void moveServoWithPID(Servo servo, int fromAngle, int toAngle)
{
  servo.write(fromAngle);
  vTaskDelay(pdMS_TO_TICKS(105));
  servo.write(toAngle);
}

void vMoveServoTask(void *pvParameters)
{
  Face *face = (Face *)pvParameters; // Convertir el puntero genérico a Face
  while (1)
  {
    Serial.println("DRIVER Motor");
    Serial.println(face->Behavior.CurrentEmotion);
    if (currentState == MOVE || currentState == NORMAL)
    {
      switch (face->Behavior.CurrentEmotion)
      {

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
        ServoDerecha.write(0);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoDerecha.write(45);
        ServoIzquierda.write(45);
        vTaskDelay(pdMS_TO_TICKS(505));
        ServoIzquierda.write(0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ServoDerecha.write(45);
        ServoIzquierda.write(170);
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
        vTaskDelay(pdMS_TO_TICKS(505));

        ServoDerecha.write(45);
        ServoIzquierda.write(45);
        vTaskDelay(pdMS_TO_TICKS(505));

        ServoDerecha.write(60);
        ServoIzquierda.write(60);
        vTaskDelay(pdMS_TO_TICKS(505));

        ServoDerecha.write(45);
        ServoIzquierda.write(45);
        vTaskDelay(pdMS_TO_TICKS(505));

        ServoDerecha.write(60);
        ServoIzquierda.write(60);
        vTaskDelay(pdMS_TO_TICKS(505));

        ServoDerecha.write(45);
        ServoIzquierda.write(45);
        vTaskDelay(pdMS_TO_TICKS(505));

        ServoDerecha.write(60);
        ServoIzquierda.write(60);
        break;

      default:
        break;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(13000));
  }
}

#endif