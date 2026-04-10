#pragma once

#include <Arduino.h>
#include <FlexCAN_T4.h>
#include "Task.h"
#include "Config.h"
#include "NodeData.h"  // adjust include as needed

class RecvCANTask : public Task {
private:
  CAN_message_t& msg;
  NodeData& nodeData;
  FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16>& canBus;

  void handleRelayCtrlMessage(const CAN_message_t& msg, NodeData& nodeData);

public:
  RecvCANTask(NodeData& nodeData,
              FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16>& canBus);

  void execute() override;
  const char* getName() override;
};