#include "FloatConverter.hpp"
#include <string.h>

void FloatConverter::floatToBytes(float value, uint8_t* buffer) {
    memcpy(buffer, &value, sizeof(float));
}

float FloatConverter::bytesToFloat(const uint8_t* bytes) {
    float value;
    memcpy(&value, bytes, sizeof(float));
    return value;
}