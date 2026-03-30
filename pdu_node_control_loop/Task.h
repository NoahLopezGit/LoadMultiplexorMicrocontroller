#pragma once

#include "Config.h"
#include <Arduino.h>

class Task {
public:
  uint32_t interval;
  uint32_t lastRun;

  Task(uint32_t intervalMs);

  bool isDue();
  void run();
  virtual void execute() = 0;
  virtual const char* getName() = 0;
};