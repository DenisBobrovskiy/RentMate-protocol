#ifndef HEADER_SMARTLOCKCORE_H
#define HEADER_SMARTLOCKCODE_H
#include "freertos/timers.h"

#define MOTOR_DRIVER_IN1_PIN 25
#define MOTOR_DRIVER_IN2_PIN 26

#define BAD_STATUS_LED_PIN 13
#define GOOD_STATUS_LED_PIN 14
#define ACTIVE_BUZZER_PIN 18
#define PROXIMITY_SENSOR_PIN 4
#define SWITCH_LOCK_OPEN_PIN 16
#define SWITCH_LOCK_CLOSED_PIN 17
#define POWER_LED_PIN 15 //This is powered while device is running

//Software variables
#define MAX_PASSWORD_LEN 12

#if targetDevtype==2

extern uint8_t currentDoorState; // State of proximity sensor to detect if door was closed. 0 - door open (sensor not active). 1 - door closed (sensor active)
extern uint8_t currentLockState; // State of lock limit switches to detect if lock was closed. 0 - lock open . 1 - lock closed
extern uint8_t currentPasscode[MAX_PASSWORD_LEN];
extern uint8_t currentPasscodeLen;

#endif


void rotateMotorClockwise();
void rotateMotorAnticlockwise();
int setPasscode(uint8_t passcode[MAX_PASSWORD_LEN], uint8_t passcodeLen, uint8_t saveToMemory);
void passcodeEntered(uint8_t *passcode, uint8_t passcodeLen);
void passcodeWrong();
void passcodeCorrect();
void initLock();
void goodLedTimeoutCallback(TimerHandle_t timerHandle);
void badLedTimeoutCallback(TimerHandle_t timerHandle);
void motorTimeoutCallback(TimerHandle_t timerHandle);
void IRAM_ATTR lockSwitchStateInterrupt(void *args);
void openSwitchTimerCallback(TimerHandle_t timerHandle);
void closedSwitchTimerCallback(TimerHandle_t timerHandle);
void IRAM_ATTR proximitySensorInterruptCallback(void *args);
void proximityTimerCallback(TimerHandle_t timerHandle);

#endif