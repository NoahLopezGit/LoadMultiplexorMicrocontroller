
#include "RecvCANTask.h"
#include "Config.h"

RecvCANTask::RecvCANTask(
  NodeData& nodeData,
  FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16>& canBus)
  : Task(100), nodeData(nodeData), canBus(canBus) {}

void RecvCANTask::execute() {
  if (canBus.read(msg)) {
    if (msg.id == (0x00)) {
      handleRelayCtrlMessage(msg, nodeData);

      if (SERIALPRINT) {
        Serial.println("Received CAN Control message");
      }
    }
  }
}

const char* RecvCANTask::getName() {
  return "RecvCAN";
}

void RecvCANTask::handleRelayCtrlMessage(const CAN_message_t& msg, NodeData& nodeData) {
  // check if control msg if for this node with first byte as node id
  if (msg.buf[0] != nodeData.id) {
    return;
  }

  // set relay based on 2nd byte
  // relay setpoint buffer is 00000000, each bit corresponds to a relay, 1 for high and 0 for low
  for (int i = 0; i < 4; i++) {
    nodeData.relaySetpointIsHigh[i] = msg.buf[1] & (1 << i);
  }
}