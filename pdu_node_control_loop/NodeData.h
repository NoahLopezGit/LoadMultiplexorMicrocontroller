#pragma once

#include <Arduino.h>  // for uint8_t, uint32_t

// state definitions
enum NodeState : uint8_t {
  NODE_INIT,
  NODE_ACTIVE,
  NODE_FAULT
};

struct NodeData {
  uint8_t id;
  uint8_t schema_version;
  NodeState state;  // 0 boot, 1 active, 2 fault

  uint16_t current[4];

  bool relaySetpointIsHigh[4];
  bool relayStateIsHigh[4];
  uint32_t relaySetpointUpdateTime;
};