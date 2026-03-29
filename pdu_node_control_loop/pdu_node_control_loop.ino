#include <ArduinoJson.h>

#define SERIALPRINT true  // set false if you don't want serial prints

// state struct (shared)
#define BOOT 0
#define ACTIVE 1
#define FAULT 2

struct NodeData {
  String id;
  int status;  // 0 boot, 1 active, 2 fault

  float current[4];
  uint32_t currentUpdateTime = 0;

  bool relaySetpointIsHigh[4];
  bool relayStateIsHigh[4];
  uint32_t relaySetpointUpdateTime;  // this must be set to update relay, call updateRelaySetpoints
};

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

      uint32_t start = micros();  // start timer

      execute();

      uint32_t duration = micros() - start;  // elapsed time

#if SERIALPRINT
      Serial.print("[Task] ");
      Serial.print(getName());
      Serial.print(" took ");
      Serial.print(duration);
      Serial.println(" us");
#endif
    }
  }

  virtual void execute() = 0;

  virtual const char* getName() = 0;
};

// CheckStatus definition
class CheckStatusTask : public Task {
private:
  NodeData& nodeData;

public:
  CheckStatusTask(NodeData& state)
    : Task(1000), nodeData(state) {}

  void execute() override {
    // check overcurrent
    for (int i = 0; i < 4; i++) {
      if (nodeData.current[i] > 10) {
        nodeData.status = FAULT;
        break;
      }
    }
  }

  const char* getName() override {
    return "CheckStatus";
  }
};

// HeartBeatTask definition
class HeartBeatTask : public Task {
private:
  NodeData& nodeData;

public:
  HeartBeatTask(NodeData& state)
    : Task(1000), nodeData(state) {}

  void execute() override {
    JsonDocument doc;

    const String statusStr =
      (nodeData.status == 0)   ? "Boot"
      : (nodeData.status == 1) ? "Active"
      : (nodeData.status == 2) ? "Fault"
                                : "Unknown";

    doc["status"] = statusStr;
    doc["uptime"] = millis();
    doc["device"] = nodeData.id;

    serializeJson(doc, Serial);
    Serial.println();  // newline
  }

  const char* getName() override {
    return "HeartBeat";
  }
};

// CurrentCheckTask definition
float getCurrent(int pin) {               // current check definition //todo can definitely make this faster
  int raw = analogRead(pin);              // 0–1023
  float voltage = raw * (5.0 / 1023.0);   // convert to volts
  float current = voltage / (0.01 * 25);  // 25x amp of .01 Ohm resistor
  return current;
}

class CurrentCheckTask : public Task {
private:
  NodeData& nodeData;

public:
  CurrentCheckTask(NodeData& state)
    : Task(100), nodeData(state) {}

  void execute() override {
    // current measurement pins are as follows: A1, A2, A3, A4
    nodeData.current[0] = getCurrent(A1);
    nodeData.current[1] = getCurrent(A2);
    nodeData.current[2] = getCurrent(A3);
    nodeData.current[3] = getCurrent(A4);

    nodeData.currentUpdateTime = millis();

    // optinal, print current
    if (SERIALPRINT) {
      Serial.print(nodeData.current[0]);
      Serial.print(" A, ");
      Serial.print(nodeData.current[1]);
      Serial.print(" A, ");
      Serial.print(nodeData.current[2]);
      Serial.print(" A, ");
      Serial.print(nodeData.current[3]);
      Serial.println(" A");
    }
  }

  const char* getName() override {
    return "CurrentCheck";
  }
};

// relaySetpointTask definition
enum RelayCtrlState {
  IDLE,
  WAITING_FOR_ACTUATION,
  CHECKING_ACTUATION
};

// relaySetpoint updater
void updateRelaySetpoints(NodeData& state, bool relaySetpointIsHigh[4]) {
  for (int i = 0; i < 4; i++) {
    state.relaySetpointIsHigh[i] = relaySetpointIsHigh[i];
  }
  state.relaySetpointUpdateTime = millis();
}

// TODO change to actual DI pins when measurement circuit is physically added
class RelayCtrlTask : public Task {
private:
  NodeData& nodeData;
  int relayCtrlPins[4] = { 0, 1, 2, 3 };         // DIO pins are 0,1,2,3
  int relayMeasurementPins[4] = { 0, 1, 2, 3 };  // DI pins are 0,1,2,3 (will be updated)
  unsigned long waitStartTime;
  RelayCtrlState ctrlState = IDLE;
  bool relaySetpointIsHighSnapshot[4];
  uint32_t waitDuration = 20;

public:
  RelayCtrlTask(NodeData& nodeData)
    : Task(25), nodeData(nodeData) {}

  void begin() {
    for (int i = 0; i < 4; i++) {
      pinMode(relayCtrlPins[i], OUTPUT);
      digitalWrite(relayCtrlPins[i], LOW);  // set default state
      // pinMode(relayMeasurementPins[i], INPUT); // todo add back in when input pins are separate
    }
  }

  void execute() override {
    switch (ctrlState) {
      case IDLE:
        {
          // if rly ctrl flag set, actuate rly
          // todo how do we know if it isn't being set or it has been update since?
          bool relayWasUpdated = false;
          for (int i = 0; i < 4; i++) {
            if (nodeData.relaySetpointIsHigh[i] != nodeData.relayStateIsHigh[i]) {
              digitalWrite(
                relayCtrlPins[i],
                nodeData.relaySetpointIsHigh[i] ? HIGH : LOW);
              relayWasUpdated = true;
            }
          }

          if (relayWasUpdated) {
            for (int i = 0; i < 4; i++) {
              relaySetpointIsHighSnapshot[i] = nodeData.relaySetpointIsHigh[i];
            }
            waitStartTime = millis();
            ctrlState = WAITING_FOR_ACTUATION;
          }

          if (SERIALPRINT) {
            Serial.print("Relay Setpoint: ");
            for (int i = 0; i < 4; i++) {
              Serial.print(nodeData.relaySetpointIsHigh[i]);
              Serial.print(" ");
            }
            Serial.println();

            Serial.print("Relay State: ");
            for (int i = 0; i < 4; i++) {
              Serial.print(nodeData.relayStateIsHigh[i]);
              Serial.print(" ");
            }
            Serial.println();
          }

          break;
        }

      case WAITING_FOR_ACTUATION:
        {
          if (millis() - waitStartTime >= waitDuration) {
            ctrlState = CHECKING_ACTUATION;
          }
          break;
        }


      case CHECKING_ACTUATION:
        {
          bool snapshotInvalid = false;
          for (int i = 0; i < 4; i++) {
            if (relaySetpointIsHighSnapshot[i] != nodeData.relaySetpointIsHigh[i]) {
              snapshotInvalid = true;
              break;
            }
          }

          if (snapshotInvalid) {
            ctrlState = IDLE;
            break;
          }

          for (int i = 0; i < 4; i++) {
            // update relay states
            bool relayStateIsHigh = (digitalRead(relayMeasurementPins[i]) == HIGH);  // TODO this isn't a true readback until DI measurement is physically configured
            nodeData.relayStateIsHigh[i] = relayStateIsHigh;

            // check for fault
            if (relaySetpointIsHighSnapshot[i] != relayStateIsHigh) {
              nodeData.status = FAULT;
            }
          }

          ctrlState = IDLE;
          break;
        }
    }
  }

  const char* getName() override {
    return "RelayCtrl";
  }
};

// ToggleTestTask
class ToggleTestTask : public Task {
private:
  NodeData& nodeData;
  uint8_t currentIndex = 0;
  static const uint8_t NUM_RELAYS = 4;

public:
  ToggleTestTask(NodeData& state)
    : Task(1000), nodeData(state) {}

  void execute() override {
    // // 5s single relay toggle test
    // nodeData.relaySetpointIsHigh[0] = nodeData.relaySetpointIsHigh[0] ? false : true;

    // relay toggle sequence test
    // Turn all relays off
    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
      nodeData.relaySetpointIsHigh[i] = false;
    }

    // Turn on only the current relay
    nodeData.relaySetpointIsHigh[currentIndex] = true;

    // Advance to next state for next scheduler call
    currentIndex++;
    if (currentIndex >= NUM_RELAYS) {
      currentIndex = 0;
    }
  }

  const char* getName() override {
    return "ToggleTest";
  }
};

// init state and config
NodeData nodeData{
  "NODE-1",                        //id
  BOOT,                            //status
  { 0.0f, 0.0f, 0.0f, 0.0f },      // current
  0,                               // currentUpdateTime
  { false, false, false, false },  // relaySetpointIsHigh
  { false, false, false, false },  // relayStateIsHigh
  0                                // relaySetpointUpdateTime
};

// init tasks
CheckStatusTask checkstatus(nodeData);
HeartBeatTask heartbeat(nodeData);
CurrentCheckTask currentcheck(nodeData);
RelayCtrlTask relayCtrl(nodeData);
ToggleTestTask toggleTest(nodeData);

void setup() {
  Serial.begin(115200);
  relayCtrl.begin();

  nodeData.status = ACTIVE;  // after boot set status to active

  pinMode(LED_BUILTIN, OUTPUT);  // turn on status LED to show successfull boot
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  // maybe handle CAN bus

  // maybe actuate relay
  relayCtrl.run();

  // maybe get current
  currentcheck.run();

  // maybe check status
  checkstatus.run();

  // maybe send heartbeat
  heartbeat.run();

  // maybe run relay toggle sequence
  toggleTest.run();

  // maybe send telemetry
}
