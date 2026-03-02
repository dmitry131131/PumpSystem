#pragma once

// A4988 pinout
const unsigned DIR_PIN    =  2;   // Pin DIR
const unsigned STEP_PIN   =  3;   // Pin STEP
const unsigned MS1_PIN    =  4;   // MS1
const unsigned MS2_PIN    =  5;   // MS2
const unsigned MS3_PIN    =  6;   // MS3
const unsigned ENABLE_PIN =  7;   // Pin ENABLE

// Engine settings 
const unsigned STEPS_PER_REV = 200;    // 200 steps per rotation (default NEMA17)

// MCP2515 pinout
const unsigned CS_PIN = 10; //  CS pin
const unsigned INT_PIN = 9; //  Interrupt pin