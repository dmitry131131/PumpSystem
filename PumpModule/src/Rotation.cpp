#include <Arduino.h>
#include "Rotation.hpp"
#include "Config.hpp"

const unsigned long MINUTE = 60000000L; // Microseconds in 1 minute

namespace {
void impuls(int microseconds) {
    int impuls_duration = microseconds > 2 ? microseconds : 2; 

    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(impuls_duration);
    digitalWrite(STEP_PIN, LOW);
}
}

// Fuction that enable rotation for the time miliseconds
void rotation(Direction direction, unsigned RPM, double time) {
    digitalWrite(DIR_PIN, direction); // Set direction

    unsigned long impuls_period = MINUTE / (RPM * STEPS_PER_REV * get_microstepping_coeff());
    double current_time = 0;        // In miliseconds

    digitalWrite(ENABLE_PIN, LOW);  // Enable driver

    while (current_time < time) {
        impuls(10);
        delayMicroseconds(impuls_period);

        current_time += static_cast<double>(impuls_period) / 1000;
    }
}

void rotation(Direction direction, unsigned RPM) {
    digitalWrite(DIR_PIN, direction); // Set direction
    unsigned long impuls_period = MINUTE / (RPM * STEPS_PER_REV * get_microstepping_coeff());
    digitalWrite(ENABLE_PIN, LOW);  // Enable driver

    while (true) {    
    impuls(10);
    delayMicroseconds(impuls_period);
    }
}

unsigned get_microstepping_coeff() {
  int MS1 = digitalRead(MS1_PIN);
  int MS2 = digitalRead(MS2_PIN);
  int MS3 = digitalRead(MS3_PIN);

  if (!(MS1 || MS2 || MS3)) {
    return 1;
  }

  return 2 * (MS1 | (MS2 << 1U) | (MS3 << 2));
}

void set_microstepping_coeff(unsigned coeff) {
    switch (coeff)
    {
    case 1:
        digitalWrite(MS1_PIN, LOW);
        digitalWrite(MS2_PIN, LOW);
        digitalWrite(MS3_PIN, LOW);
        break;
    case 2:
        digitalWrite(MS1_PIN, HIGH);
        digitalWrite(MS2_PIN, LOW);
        digitalWrite(MS3_PIN, LOW);
        break;
    case 4:
        digitalWrite(MS1_PIN, LOW);
        digitalWrite(MS2_PIN, HIGH);
        digitalWrite(MS3_PIN, LOW);
        break;
    case 8:
        digitalWrite(MS1_PIN, HIGH);
        digitalWrite(MS2_PIN, HIGH);
        digitalWrite(MS3_PIN, LOW);
        break;
    case 16:
        digitalWrite(MS1_PIN, HIGH);
        digitalWrite(MS2_PIN, HIGH);
        digitalWrite(MS3_PIN, HIGH);
        break;
    
    default:
        // TODO error handler
        break;
    }
}