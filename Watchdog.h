#ifndef WATCHDOG_H
#define WATCHDOG_H


#include <esp_task_wdt.h> // Agregar la biblioteca para el watchdog

void feedWatchdog() {
  esp_task_wdt_reset();
}

#endif