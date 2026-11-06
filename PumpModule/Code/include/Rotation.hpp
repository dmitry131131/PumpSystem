// Motor rotation control

#pragma once

#include "BusConnectionConfig.h"

unsigned get_microsteping_coeff();
void set_microsteping_coeff(unsigned coeff);

void rotation(RotationDirection direction, unsigned RPM, double time);
void rotation(RotationDirection direction, unsigned RPM);
void rotate(RotationDirection direction, double degree);
void rotate(RotationDirection direction, double degree, unsigned RPM);
