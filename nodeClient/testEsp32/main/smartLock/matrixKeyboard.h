#ifndef HEADER_SMARTLOCK_H
#define HEADER_SMARTLOCK_H

#include "driver/gpio.h"
#include "../client.h"

// Keypad pins
#define KEYPAD_ROW1_PIN 22
#define KEYPAD_ROW2_PIN 23
#define KEYPAD_ROW3_PIN 19
#define KEYPAD_ROW4_PIN 21
#define KEYPAD_COL1_PIN 33
#define KEYPAD_COL2_PIN 27
#define KEYPAD_COL3_PIN 32

//Keypad config 
#define KEYPAD_DEBOUNCE_MS 1000

void initKeypad();
void IRAM_ATTR keypadInterruptHandler(void *args);
void debounceTimerCallback(TimerHandle_t handle);

#endif