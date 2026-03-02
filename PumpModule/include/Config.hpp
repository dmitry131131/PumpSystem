#pragma once

// A4988 pinout
const unsigned DIR_PIN    =  2;   // Пин DIR
const unsigned STEP_PIN   =  3;   // Пин STEP
const unsigned MS1_PIN    =  4;   // MS1
const unsigned MS2_PIN    =  5;   // MS2
const unsigned MS3_PIN    =  6;   // MS3
const unsigned ENABLE_PIN =  7;   // Пин ENABLE

// Engine settings 
const unsigned STEPS_PER_REV = 200;    // 200 steps per rotation (default NEMA17)