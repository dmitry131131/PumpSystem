// Motor rotation control

#pragma once

#include "BusConnectionConfig.h"

unsigned get_microsteping_coeff();
void set_microsteping_coeff(unsigned coeff);

void rotate(RotationDirection direction, double degree, unsigned RPM, bool (*serviceCAN)() = nullptr);
