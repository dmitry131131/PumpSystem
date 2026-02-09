#include <Arduino.h>
#include "Config.hpp"
#include "Rotation.hpp"
#include "BusConnection.hpp"

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
  set_microstepping_coeff(1);
}

void loop() {
  rotation(Direction::REVERSE, 120);
}

