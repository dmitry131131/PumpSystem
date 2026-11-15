// Connection with CAN interface to master chip

#pragma once

#include <SPI.h>
#include <mcp2515.h>

#include "BusConnectionConfig.h"

MCP2515::ERROR CANInitialization(MCP2515& mcp2515);
bool ReadQueuedCANMessage(can_frame& frame);
bool QueueCANMessage(const can_frame& frame);
MCP2515::ERROR CANTxRuntime(MCP2515& mcp2515);
bool QueueAttendanceResponse(PumpState state);
bool QueueSwitchEventMessage(MessageType msgType);
