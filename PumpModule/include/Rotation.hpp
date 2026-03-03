// Motor rotation control

#pragma once

enum Direction : unsigned char {
  REVERSE = 0,
  FORWARD = 1
};

unsigned get_microsteping_coeff();
void set_microsteping_coeff(unsigned coeff);

void rotation(Direction direction, unsigned RPM, double time);
void rotation(Direction direction, unsigned RPM);
void rotate(Direction direction, double degree);
void rotate_full(Direction direction, unsigned count);
