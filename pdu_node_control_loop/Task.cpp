#include "Task.h"
#include "Config.h"
#include <Arduino.h>

Task::Task(uint32_t intervalMs)
  : interval(intervalMs), lastRun(0) {}

bool Task::isDue() {
  return millis() - lastRun >= interval;
}

void Task::run() {
  if (isDue()) {
    lastRun = millis();

#if SERIALPRINT
    uint32_t start = micros();  // start timer
#endif

    execute();

#if SERIALPRINT
    uint32_t duration = micros() - start;  // elapsed time
    Serial.print("[Task] ");
    Serial.print(getName());
    Serial.print(" took ");
    Serial.print(duration);
    Serial.println(" us");
#endif
  }
}