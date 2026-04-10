#include "SendCurrentTask.h"
#include "Config.h"

SendCurrentTask::SendCurrentTask(
  NodeData& nodeData,
  FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16>& canBus)
  : Task(100), nodeData(nodeData), canBus(canBus) {}

void SendCurrentTask::execute() {
  CAN_message_t txMsg;
  txMsg.id = msg_base_id + nodeData.id;
  txMsg.len = 8;
  txMsg.flags.extended = 0;

  uint8_t payload[8];
  packCurrentMessage(nodeData, payload);

  for (uint8_t i = 0; i < 8; ++i) {
    txMsg.buf[i] = payload[i];
  }

  canBus.write(txMsg);

  if (SERIALPRINT) {
    Serial.println("Sending Current");
  }
}

const char* SendCurrentTask::getName() {
  return "SendCurrent";
}

void SendCurrentTask::packCurrentMessage(const NodeData& nodeData, uint8_t payload[8]) {
  payload[0] = nodeData.current[0] & 0xFF;  // current 1 low byte
  payload[1] = nodeData.current[0] >> 8;    // current 1 high byte
  payload[2] = nodeData.current[1] & 0xFF;  // current 2 low byte
  payload[3] = nodeData.current[1] >> 8;    // current 2 high byte
  payload[4] = nodeData.current[2] & 0xFF;  // current 3 low byte
  payload[5] = nodeData.current[2] >> 8;    // current 3 high byte
  payload[6] = nodeData.current[3] & 0xFF;  // current 4 low byte
  payload[7] = nodeData.current[3] >> 8;    // current 4 high byte
}