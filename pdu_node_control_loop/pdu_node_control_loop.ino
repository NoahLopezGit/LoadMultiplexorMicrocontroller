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
class RelayCtrlTask : public Task {
private:
  NodeData& nodeData;
  int relayCtrlPins[4] = { 0, 1, 2, 3 };         // DIO pins are 0,1,2,3
  bool relaySetpointSnapshot[4];

public:
  RelayCtrlTask(NodeData& nodeData)
    : Task(25), nodeData(nodeData) {}

  void begin() {
    for (int i = 0; i < 4; i++) {
      pinMode(relayCtrlPins[i], OUTPUT);
      digitalWrite(relayCtrlPins[i], LOW);  // set default state
    }
  }

  void execute() override {
    bool relayWasUpdated = false;
    for (int i = 0; i < 4; i++) {
      if (nodeData.relaySetpoint[i] != relaySetpointSnapshot[i]) {
        digitalWrite(
          relayCtrlPins[i],
          nodeData.relaySetpoint[i] ? HIGH : LOW
        );
        relaySetpointSnapshot[i] = nodeData.relaySetpoint[i];
        relayWasUpdated = true;
      }
    }
      
    if (SERIALPRINT && relayWasUpdated) {
      Serial.print("Relay Setpoint: ");
      for (int i = 0; i < 4; i++) {
        Serial.print(nodeData.relaySetpoint[i]);
        Serial.print(" ");
      }
      Serial.println();
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
    // nodeData.relaySetpoint[0] = nodeData.relaySetpoint[0] ? false : true;

    // relay toggle sequence test
    // Turn all relays off
    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
      nodeData.relaySetpoint[i] = false;
    }

    // Turn on only the current relay
    nodeData.relaySetpoint[currentIndex] = true;

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
  NODEID,                          //id
  1,                               //schema_version
  nodeState,                       //state
  { 0, 0, 0, 0 },                  // current
  { false, false, false, false },  // relaySetpoint
};

// init tasks
CurrentCheckTask currentcheck(nodeData);
RelayCtrlTask relayCtrl(nodeData);

#if TOGGLETEST
ToggleTestTask toggleTest(nodeData);
#endif


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

#if TOGGLETEST
  // maybe run relay toggle sequence
  toggleTest.run();
#endif

  // maybe send telemetry
  sendDeviceStatus.run();
  sendCurrent.run();
}
