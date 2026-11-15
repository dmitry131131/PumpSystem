#pragma once

#include <stdint.h>

class FloatConverter final {
public:
    /**
     * @brief Converts an array of 4 bytes (uint8_t[4]) to float
     * @param bytes Array of 4 bytes to convert
     * @return Float value
     */
    static float bytesToFloat(const uint8_t* bytes);
    
    /**
     * @brief Converts float to an array of 4 bytes with copying to buffer
     * @param value Float value to convert
     * @param buffer Buffer to store bytes (must be at least 4 bytes)
     */
    static void floatToBytes(float value, uint8_t* buffer);
    
private:
    // Private constructor as this is a utility class
    FloatConverter() = default;
};