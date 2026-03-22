#include <ArduinoJson.h>

#define DEVICE "NODE-1"  // todo what is the better dynamic way to do this?

// state struct (shared)
struct NodeState {
  String id;
  int status;  // 0 boot, 1 active, 2 fault
  float current[4];
  uint32_t timestamp;
  bool valid;
};

NodeState nodeState = { 0, DEVICE, { 0, 0, 0, 0 }, 0, false };  // init state

// base cooperative task to be inherited by actual tasks
class Task {
public:
  uint32_t interval;
  uint32_t lastRun;

  Task(uint32_t intervalMs)
    : interval(intervalMs), lastRun(0) {}

  bool isDue() {
    return millis() - lastRun >= interval;
  }

  void run() {
    if (isDue()) {
      lastRun = millis();
      execute();
    }
  }

  virtual void execute() = 0;
};

// check status task
void setStatus(int status) {
  nodeState.status = status;
}

void checkStatus() {
  // check overcurrent
  for (int i = 0; i < 4; i++) {
    nodeState.status = (nodeState.current[i] > 10) ? 2 : nodeState.status;
  }
}

class CheckStatusTask : public Task {
public:
  CheckStatusTask()
    : Task(1000) {}

  void execute() override {
    checkStatus();
  }
};

// heart beat task definition
void sendHeartbeat() {
  JsonDocument doc;

  const String statusStr =
    (nodeState.status == 0)   ? "Boot"
    : (nodeState.status == 1) ? "Active"
    : (nodeState.status == 2) ? "Fault"
                              : "Unknown";

  doc["status"] = statusStr;
  doc["uptime"] = millis();
  doc["device"] = nodeState.id;

  serializeJson(doc, Serial);
  Serial.println();  // newline
}

class HeartBeatTask : public Task {
public:
  HeartBeatTask()
    : Task(1000) {}

  void execute() override {
    sendHeartbeat();
  }
};

// current check definition //todo can definitely make this faster
float getCurrent(int pin) {
  int raw = analogRead(pin);              // 0–1023
  float voltage = raw * (5.0 / 1023.0);   // convert to volts
  float current = voltage / (0.01 * 25);  // 25x amp of .01 Ohm resistor
  return current;
}

class CurrentCheckTask : public Task {
public:
  int measurementPin;
  int measurementIdx;

  CurrentCheckTask(int measurementPin, int measurementIdx)
    : Task(100), measurementPin(measurementPin), measurementIdx(measurementIdx) {}

  void execute() override {
    float current = getCurrent(measurementPin);
    nodeState.current[measurementIdx] = current;
    nodeState.timestamp = millis();
    nodeState.valid = true;
    Serial.println(current);
  }
};

// init tasks
CheckStatusTask checkstatus;
HeartBeatTask heartbeat;
CurrentCheckTask currentcheckA1(A1, 0);  // current measurement pins are as follows: A1, A2, A3, A4
CurrentCheckTask currentcheckA2(A2, 1);
CurrentCheckTask currentcheckA3(A3, 2);
CurrentCheckTask currentcheckA4(A4, 3);

void setup() {
  // put your setup code here, to run once:

  setStatus(1);  // at end of setup set status to active
}

void loop() {
  // maybe handle CAN bus

  // maybe actuate relay

  // maybe get current
  currentcheckA1.run();
  currentcheckA2.run();
  currentcheckA3.run();
  currentcheckA4.run();

  // maybe check status
  checkstatus.run();

  // maybe send heartbeat
  heartbeat.run();

  // maybe send telemetry
}
