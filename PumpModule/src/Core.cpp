#include <Arduino.h>
#include "Config.hpp"
#include "Rotation.hpp"
#include "BusConnection.hpp"

#include <SPI.h>
#include <mcp2515.h>

struct can_frame canMsg;
MCP2515 mcp2515(10);

void setup() {
  // Set pins as output
  // pinMode(DIR_PIN,    OUTPUT);
  // pinMode(STEP_PIN,   OUTPUT);
  // pinMode(MS1_PIN,    OUTPUT);
  // pinMode(MS2_PIN,    OUTPUT);
  // pinMode(MS3_PIN,    OUTPUT);
  // pinMode(ENABLE_PIN, OUTPUT);
  
  // Disable driver (active LOW)
  // digitalWrite(ENABLE_PIN, HIGH);

  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  digitalWrite(6, HIGH);

  // Disable microstepping
  // set_microstepping_coeff(1);

  // CAN configuration
  mcp2515.reset();
  mcp2515.setBitrate(CAN_50KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();
}

void loop() {

  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    digitalWrite(5, HIGH);
    delay(100);
    digitalWrite(5, LOW);
    delay(100);
  }
  
  // rotation(Direction::FORWARD, 120);
}

