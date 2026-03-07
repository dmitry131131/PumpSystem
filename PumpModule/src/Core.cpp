#include <Arduino.h>
#include "Config.hpp"
#include "Rotation.hpp"
#include "BusConnection.hpp"

MCP2515 mcp2515(CS_PIN);
bool registeredInCAN = false; 

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
  // Set microsteping
  set_microsteping_coeff(16);

  // Delay for all device initialization
  delay(2000);

  // CAN configuration
  if(CANInitialization(mcp2515) == MCP2515::ERROR_OK) {
    registeredInCAN = true;
  }
}

void loop() {
  if (!registeredInCAN) {
    return;
  }

  // rotate(Direction::FORWARD, 442);
  rotate(Direction::FORWARD, 50);
  delay(5000);
}

