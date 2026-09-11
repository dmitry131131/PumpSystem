// Connection with CAN interface to master chip

#pragma once

#include <SPI.h>
#include <mcp2515.h>

#include "BusConnectionConfig.h"

MCP2515::ERROR CANInitialization(MCP2515& mcp2515);
MCP2515::ERROR SendAttendanceResponse(MCP2515 &mcp2515);
MCP2515::ERROR SendSwitchEventMessage(MCP2515 &mcp2515, MessageType msgType);
void CanInterrupt();