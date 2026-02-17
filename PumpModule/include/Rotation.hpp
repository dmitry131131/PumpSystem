// Motor rotation control

#pragma once

enum Direction : unsigned char {
  FORWARD = 0,
  REVERSE = 1
};

unsigned get_microstepping_coeff();
void set_microstepping_coeff(unsigned coeff);

void rotation(Direction direction, unsigned RPM, unsigned long time);
void rotation(Direction direction, unsigned RPM);
