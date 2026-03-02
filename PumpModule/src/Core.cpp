#include <Arduino.h>
#include "Config.hpp"
#include "Rotation.hpp"
#include "BusConnection.hpp"

#include <SPI.h>
#include <mcp2515.h>

struct can_frame canMsg;
MCP2515 mcp2515(CS_PIN);

void setup() {
  // Set pins as output
  pinMode(DIR_PIN,    OUTPUT);
  pinMode(STEP_PIN,   OUTPUT);
  pinMode(MS1_PIN,    OUTPUT);
  pinMode(MS2_PIN,    OUTPUT);
  pinMode(MS3_PIN,    OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  
  // Disable driver (active LOW)
  digitalWrite(ENABLE_PIN, HIGH);

  // Disable microstepping
  set_microstepping_coeff(16);

  // CAN configuration
  mcp2515.reset();
  mcp2515.setBitrate(CAN_50KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();
}

void loop() {

  // if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
  //   rotation(Direction::FORWARD, 6, 300);
  // }

  rotate(Direction::FORWARD, 442);
  delay(5000);
  // rotation(Direction::REVERSE, 6, 1000);
  // delay(3000);
  
  // rotate(Direction::FORWARD, 180);
  // delay(3000);
  // rotate(Direction::REVERSE, 180);
  // delay(3000);

  // rotation(Direction::REVERSE, 6);
}

