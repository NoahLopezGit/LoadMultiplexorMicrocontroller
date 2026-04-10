#include <ArduinoJson.h>
#include <FlexCAN_T4.h>
#include "Config.h"
#include "Task.h"
#include "NodeData.h"
#include "SendDeviceStatusTask.h"
#include "SendCurrentTask.h"
#include "RecvCANTask.h"

// CurrentCheckTask definition
uint16_t getCurrent_mA(int pin) {  // returns current in mA (10000 mA max).
  uint16_t raw = analogRead(pin);

  // Step-by-step integer math:
  // voltage (mV) = raw * 3300 / 1023 = raw
  // current (mA) = voltage / 0.25 = voltage * 4

  // overflow check... fits in uint16_t
  // 1023 * 3300 * 4 = 13,448,400  // fits in uint32_t
  // multiplication happens in uint32_t and then is divided down to fit in uint16_t

  return (raw * 3300UL * 4) / 1023UL;
}

class CurrentCheckTask : public Task {
private:
  NodeData& nodeData;

public:
  CurrentCheckTask(NodeData& state)
    : Task(100), nodeData(state) {}

  void execute() override {
    // current measurement pins are as follows: A1, A2, A3, A4
    nodeData.current[0] = getCurrent_mA(A1);
    nodeData.current[1] = getCurrent_mA(A2);
    nodeData.current[2] = getCurrent_mA(A3);
    nodeData.current[3] = getCurrent_mA(A4);

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
              nodeData.state = NODE_FAULT;
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
NodeState nodeState = NODE_INIT;
NodeData nodeData{
  0,                               //id
  1,                               //schema_version
  nodeState,                       //state
  { 0, 0, 0, 0 },                  // current
  { false, false, false, false },  // relaySetpointIsHigh
  { false, false, false, false },  // relayStateIsHigh
  0                                // relaySetpointUpdateTime
};

// init tasks
CurrentCheckTask currentcheck(nodeData);
RelayCtrlTask relayCtrl(nodeData);
ToggleTestTask toggleTest(nodeData);

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> canBus;
SendDeviceStatusTask sendDeviceStatus(nodeData, canBus);
SendCurrentTask sendCurrent(nodeData, canBus);
RecvCANTask recvCAN(nodeData, canBus);

void setup() {
  Serial.begin(115200);
  relayCtrl.begin();

  canBus.begin();
  canBus.setBaudRate(500000);

  nodeData.state = NODE_ACTIVE;  // after boot set state to active

  pinMode(LED_BUILTIN, OUTPUT);  // turn on status LED to show successfull boot
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  // maybe handle CAN bus
  recvCAN.run();

  // maybe actuate relay
  relayCtrl.run();

  // maybe get current
  currentcheck.run();

  // maybe run relay toggle sequence
  toggleTest.run();

  // maybe send telemetry
  sendDeviceStatus.run();
  sendCurrent.run();
}
