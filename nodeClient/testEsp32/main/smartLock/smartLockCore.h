#ifndef HEADER_SMARTLOCKCORE_H
#define HEADER_SMARTLOCKCODE_H

#define MAX_PASSWORD_LEN 12

#define MOTOR_DRIVER_IN1_PIN 25
#define MOTOR_DRIVER_IN2_PIN 26

#define BAD_STATUS_LED_PIN 13
#define GOOD_STATUS_LED_PIN 14


void passcodeEntered(uint8_t *passcode, uint8_t passcodeLen);
void passcodeWrong();
void passcodeCorrect();
void initLock();
void goodLedTimeoutCallback(TimerHandle_t timerHandle);
void badLedTimeoutCallback(TimerHandle_t timerHandle);
void motorTimeoutCallback(TimerHandle_t timerHandle);

#endif