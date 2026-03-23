#include <ArduinoJson.h>

#define SERIALPRINT true  // set false if you don't want serial prints

// state struct (shared)
#define BOOT 0
#define ACTIVE 1
#define FAULT 2

struct NodeState {
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
      execute();
    }
  }

  virtual void execute() = 0;
};

// CheckStatus definition
class CheckStatusTask : public Task {
private:
  NodeState& nodeState;

public:
  CheckStatusTask(NodeState& state)
    : Task(1000), nodeState(state) {}

  void execute() override {
    // check overcurrent
    for (int i = 0; i < 4; i++) {
      if (nodeState.current[i] > 10) {
        nodeState.status = FAULT;
        break;
      }
    }
  }
};

// HeartBeatTask definition
class HeartBeatTask : public Task {
private:
  NodeState& nodeState;

public:
  HeartBeatTask(NodeState& state)
    : Task(1000), nodeState(state) {}

  void execute() override {
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
  NodeState& nodeState;

public:
  CurrentCheckTask(NodeState& state)
    : Task(100), nodeState(state) {}

  void execute() override {
    // current measurement pins are as follows: A1, A2, A3, A4
    nodeState.current[0] = getCurrent(A1);
    nodeState.current[1] = getCurrent(A2);
    nodeState.current[2] = getCurrent(A3);
    nodeState.current[3] = getCurrent(A4);

    nodeState.currentUpdateTime = millis();

    // optinal, print current
    if (SERIALPRINT) {
      Serial.print(nodeState.current[0]);
      Serial.print(" A, ");
      Serial.print(nodeState.current[1]);
      Serial.print(" A, ");
      Serial.print(nodeState.current[2]);
      Serial.print(" A, ");
      Serial.print(nodeState.current[3]);
      Serial.println(" A");
    }
  }
};

// relaySetpointTask definition
enum RelayCtrlState {
  IDLE,
  WAITING_FOR_ACTUATION,
  CHECKING_ACTUATION
};

// relaySetpoint updater
void updateRelaySetpoints(NodeState& state, bool relaySetpointIsHigh[4]) {
  for (int i = 0; i < 4; i++) {
    state.relaySetpointIsHigh[i] = relaySetpointIsHigh[i];
  }
  state.relaySetpointUpdateTime = millis();
}

// TODO change to actual DI pins when measurement circuit is physically added
class RelayCtrlTask : public Task {
private:
  NodeState& nodeState;
  int relayCtrlPins[4] = { 0, 1, 2, 3 };         // DIO pins are 0,1,2,3
  int relayMeasurementPins[4] = { 0, 1, 2, 3 };  // DI pins are 0,1,2,3 (will be updated)
  unsigned long waitStartTime;
  RelayCtrlState ctrlState = IDLE;
  bool relaySetpointIsHighSnapshot[4];

public:
  RelayCtrlTask(NodeState& nodeState)
    : Task(100), nodeState(nodeState) {}

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
            if (nodeState.relaySetpointIsHigh[i] != nodeState.relayStateIsHigh[i]) {
              digitalWrite(
                relayCtrlPins[i],
                nodeState.relaySetpointIsHigh[i] ? HIGH : LOW);
              relayWasUpdated = true;
            }
          }

          if (relayWasUpdated) {
            for (int i = 0; i < 4; i++) {
              relaySetpointIsHighSnapshot[i] = nodeState.relaySetpointIsHigh[i];
            }
            waitStartTime = millis();
            ctrlState = WAITING_FOR_ACTUATION;
          }

          if (SERIALPRINT) {
            Serial.print("Relay Setpoint: ");
            for (int i = 0; i < 4; i++) {
              Serial.print(nodeState.relaySetpointIsHigh[i]);
              Serial.print(" ");
            }
            Serial.println();

            Serial.print("Relay State: ");
            for (int i = 0; i < 4; i++) {
              Serial.print(nodeState.relayStateIsHigh[i]);
              Serial.print(" ");
            }
            Serial.println();
          }

          break;
        }

      case WAITING_FOR_ACTUATION:
        {
          if (millis() - waitStartTime >= 500) {
            ctrlState = CHECKING_ACTUATION;
          }
          break;
        }


      case CHECKING_ACTUATION:
        {
          bool snapshotInvalid = false;
          for (int i = 0; i < 4; i++) {
            if (relaySetpointIsHighSnapshot[i] != nodeState.relaySetpointIsHigh[i]) {
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
            nodeState.relayStateIsHigh[i] = relayStateIsHigh;

            // check for fault
            if (relaySetpointIsHighSnapshot[i] != relayStateIsHigh) {
              nodeState.status = FAULT;
            }
          }

          ctrlState = IDLE;
          break;
        }
    }
  }
};

// HandleCanBus

// init state and config
NodeState nodeState{
  "NODE-1",                        //id
  BOOT,                            //status
  { 0.0f, 0.0f, 0.0f, 0.0f },      // current
  0,                               // currentUpdateTime
  { false, false, false, false },  // relaySetpointIsHigh
  { false, false, false, false },  // relayStateIsHigh
  0                                // relaySetpointUpdateTime
};

// init tasks
CheckStatusTask checkstatus(nodeState);
HeartBeatTask heartbeat(nodeState);
CurrentCheckTask currentcheck(nodeState);
RelayCtrlTask relayCtrl(nodeState);

void setup() {
  Serial.begin(115200);
  relayCtrl.begin();

  nodeState.status = ACTIVE;  // after boot set status to active
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

  // maybe send telemetry
}
