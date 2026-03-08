// Connection with CAN interface to master chip

#pragma once

#include <SPI.h>
#include <mcp2515.h>

MCP2515::ERROR CANInitialization(MCP2515& mcp2515);
MCP2515::ERROR SendAttendanceResponse(MCP2515 &mcp2515);
void CanInterrupt();