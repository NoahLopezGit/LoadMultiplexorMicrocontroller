#include "SendDeviceStatusTask.h"
#include "Config.h"

SendDeviceStatusTask::SendDeviceStatusTask(
  NodeData& nodeData,
  FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16>& canBus)
  : Task(1000), nodeData(nodeData), canBus(canBus) {}

void SendDeviceStatusTask::execute() {
  CAN_message_t txMsg;
  txMsg.id = device_status_base_id + nodeData.id;
  txMsg.len = 8;
  txMsg.flags.extended = 0;

  uint8_t payload[8];
  packDeviceStatusMessage(nodeData, payload);

  for (uint8_t i = 0; i < 8; ++i) {
    txMsg.buf[i] = payload[i];
  }

  canBus.write(txMsg);

  if (SERIALPRINT) {
    Serial.println("Sending Device Status");
  }
}

const char* SendDeviceStatusTask::getName() {
  return "SendDeviceStatus";
}

void SendDeviceStatusTask::packDeviceStatusMessage(const NodeData& nodeData, uint8_t payload[8]) {
  payload[0] = nodeData.schema_version;
  payload[1] = static_cast<uint8_t>(nodeData.state);
  payload[2] = 0;  // static_cast<uint8_t>(status.status_flags & 0xFF);
  payload[3] = 0;  // static_cast<uint8_t>((status.status_flags >> 8) & 0xFF);
  payload[4] = 0;  // static_cast<uint8_t>(status.uptime_s & 0xFF);
  payload[5] = 0;  // static_cast<uint8_t>((status.uptime_s >> 8) & 0xFF);
  payload[6] = 0;  // status.heartbeat_counter;
  payload[7] = 0;
}