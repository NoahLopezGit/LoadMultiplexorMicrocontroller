#include "CANTasks.h"
#include "Config.h"

RecvCANTask::RecvCANTask(
  NodeData& nodeData,
  FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16>& canBus)
  : Task(50), nodeData(nodeData), canBus(canBus) {}

void RecvCANTask::execute() {
  while (canBus.read(msg)) { // keep reading until buffer is clear
    if (msg.id == 0x000) {
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
    nodeData.relaySetpoint[i] = msg.buf[1] & (1 << i);
  }
}

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

  uint8_t relaySetpoints = 0;
  for (int i = 0; i < 4; i++) {
    relaySetpoints = relaySetpoints | nodeData.relaySetpoint[i] << i;
  }
  payload[2] = relaySetpoints;

  payload[3] = 0;  // static_cast<uint8_t>((status.status_flags >> 8) & 0xFF);
  payload[4] = 0;  // static_cast<uint8_t>(status.uptime_s & 0xFF);
  payload[5] = 0;  // static_cast<uint8_t>((status.uptime_s >> 8) & 0xFF);
  payload[6] = 0;  // status.heartbeat_counter;
  payload[7] = 0;
}

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