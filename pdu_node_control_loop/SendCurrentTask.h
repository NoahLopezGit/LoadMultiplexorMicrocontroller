#pragma once

#include <Arduino.h>
#include <FlexCAN_T4.h>
#include "Task.h"
#include "Config.h"
#include "NodeData.h"  // adjust include as needed

class SendCurrentTask : public Task {
private:
  NodeData& nodeData;
  FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16>& canBus;
  uint32_t msg_base_id = 0x200;

  void packCurrentMessage(const NodeData& nodeData, uint8_t payload[8]);

public:
  SendCurrentTask(NodeData& nodeData,
                       FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16>& canBus);

  void execute() override;
  const char* getName() override;
};