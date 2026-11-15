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

void rotate(RotationDirection direction, double degree, unsigned RPM, bool (*serviceCAN)()) {
    digitalWrite(DIR_PIN, direction);   // Set direction
    unsigned long impuls_period = MINUTE / (RPM * STEPS_PER_REV * get_microsteping_coeff());
    digitalWrite(ENABLE_PIN, LOW);  // Enable driver

    size_t impuls_count = static_cast<unsigned>(degree * STEPS_PER_REV * get_microsteping_coeff() / 360);
    for (size_t i = 0; i < impuls_count; ++i) {
        unsigned long step_start = micros();
        impuls(10);
        if (serviceCAN && !serviceCAN()) {
            break;
        }
        unsigned long elapsed = micros() - step_start;
        if (elapsed < impuls_period) {
            delayMicroseconds(impuls_period - elapsed);
        }
    }

    digitalWrite(ENABLE_PIN, HIGH);  // Disable driver
}

unsigned get_microsteping_coeff() {
  bool MS1 = digitalRead(MS1_PIN);
  bool MS2 = digitalRead(MS2_PIN);
  bool MS3 = digitalRead(MS3_PIN);

  unsigned combined = MS1 | MS2 << 1U | MS3 << 2U;

  switch (combined)
  {
  case 0:
    return 1;
  case 1:
    return 2;
  case 2:
    return 4;
  case 3:
    return 8;
  case 7:
    return 16;
  
  default:
    return 16;
  }
}

void set_microsteping_coeff(unsigned coeff) {
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
