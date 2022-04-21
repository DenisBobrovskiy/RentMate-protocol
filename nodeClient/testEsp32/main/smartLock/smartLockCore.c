#include <stdio.h>
#include "../client.h"
#include "smartLockCore.h"
#include "matrixKeyboard.h"
#include <memory.h>
#include "driver/dac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#define ANSI_COLOR_BLUE "\x1B[34m"
#define ANSI_COLOR_WHITE "\x1B[37m"

#define GOOD_STATUS_LED_TIMER_ID 2
#define BAD_STATUS_LED_TIMER_ID 3
#define MOTOR_ROTATION_TIMEOUT_TIMER_ID 4 //Called when motor was rotating for too long (Ideally it should turn off before the timer expires due to
// GPIO interrupt from hitting a limit switch in the mechanism)

#define GOOD_STATUS_LED_DELAY_MS 500 //How long the status LED should stay on
#define BAD_STATUS_LED_DELAY_MS 500 //How long the status LED should stay on
#define MOTOR_ROTATION_TIMEOUT_MS 1000 //How long the motor should rotate before stopping (timeout), this is a fallback mechanism in case a limit switch doesn't work

//Current lock passcode, needs to be initialized from settings files
uint8_t currentPasscode[MAX_PASSWORD_LEN] = {1,2,4,4};
uint8_t currentPasscodeLen = 4;

//Timers
TimerHandle_t goodLedTimer;
TimerHandle_t badLedTimer;
TimerHandle_t motorTimeoutTimer;


//Static function prototypes
static int printf2(char *formattedInput, ...);

void initLock(){



    initKeypad();

    //Intialize timers
    goodLedTimer = xTimerCreate("goodLedTimer", pdMS_TO_TICKS(GOOD_STATUS_LED_DELAY_MS), pdFALSE, (void*)GOOD_STATUS_LED_TIMER_ID, goodLedTimeoutCallback);
    badLedTimer = xTimerCreate("badLedTimer", pdMS_TO_TICKS(BAD_STATUS_LED_DELAY_MS), pdFALSE, (void*)BAD_STATUS_LED_TIMER_ID, badLedTimeoutCallback);
    motorTimeoutTimer = xTimerCreate("motorTimeoutTimer", pdMS_TO_TICKS(MOTOR_ROTATION_TIMEOUT_MS), pdFALSE, (void*)MOTOR_ROTATION_TIMEOUT_TIMER_ID, motorTimeoutCallback);

    //Intialize LED pins
    gpio_pad_select_gpio(GOOD_STATUS_LED_PIN);
    gpio_pad_select_gpio(BAD_STATUS_LED_PIN);
    gpio_set_direction(GOOD_STATUS_LED_PIN,GPIO_MODE_OUTPUT);
    gpio_set_direction(BAD_STATUS_LED_PIN,GPIO_MODE_OUTPUT);
    gpio_set_level(GOOD_STATUS_LED_PIN,0);
    gpio_set_level(BAD_STATUS_LED_PIN,0);
    
    //Initialize motor driver.
    gpio_pad_select_gpio(MOTOR_DRIVER_IN1_PIN);
    gpio_pad_select_gpio(MOTOR_DRIVER_IN2_PIN);
    gpio_set_direction(MOTOR_DRIVER_IN1_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(MOTOR_DRIVER_IN2_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(MOTOR_DRIVER_IN1_PIN,0);
    gpio_set_level(MOTOR_DRIVER_IN2_PIN,0);

}

//Function called when a passcode was entered from keypad. THIS RUNS INSIDE INTERRUPT SO NORMAL PRINTF CANT BE USED OR CRASH
void passcodeEntered(uint8_t *passcode, uint8_t passcodeLen){
    esp_rom_printf("Passcode entered: ");
    for(int i = 0; i<passcodeLen;i++){
        esp_rom_printf("%d",passcode[i]);
    }
    esp_rom_printf("\n");
    for(int i = 0; i<passcodeLen;i++){
        esp_rom_printf("%d",currentPasscode[i]);
    }
    esp_rom_printf("\n");

    if(passcodeLen==currentPasscodeLen){
        if(memcmp(currentPasscode,passcode,passcodeLen)==0){
            //Password entered good
            passcodeCorrect();

        }else{
            //Bad password
            passcodeWrong();
        }
    }else{
        //Bad password
        passcodeWrong();

    }
}

//Call this when passcode entered is wrong. Flash RED Led and maybe beeper
void passcodeWrong(){
    esp_rom_printf("Password entered is wrong\n");
    gpio_set_level(BAD_STATUS_LED_PIN,1);

    //Start timer to turn off LED
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    if(xTimerStartFromISR(badLedTimer,higherPriorityTaskWoken)!=pdPASS){
        //Timer failed starting
        esp_rom_printf("Bad led timeout timer failed starting\n");
    }
}

//Password correct, blink green LED and turn the motor
void passcodeCorrect(){
    esp_rom_printf("Password entered is correct\n");

    //Turn on good status LED
    gpio_set_level(MOTOR_DRIVER_IN1_PIN,1);

    //Turn on motor driver
    gpio_set_level(GOOD_STATUS_LED_PIN,1);

    //Start timer to turn off LED
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    if(xTimerStartFromISR(goodLedTimer,higherPriorityTaskWoken)!=pdPASS){
        //Timer failed starting
        esp_rom_printf("Good led timeout timer failed starting\n");
    }

    //Start timer to turn off motor driver
    if(xTimerStartFromISR(motorTimeoutTimer,higherPriorityTaskWoken)!=pdPASS){
        //Timer failed starting
        esp_rom_printf("Motor timeout timer failed starting\n");
    }
}


void goodLedTimeoutCallback(TimerHandle_t timerHandle){
    esp_rom_printf("LED GOOD timer timeout\n");
    gpio_set_level(GOOD_STATUS_LED_PIN,0);
}

void badLedTimeoutCallback(TimerHandle_t timerHandle){
    esp_rom_printf("LED BAD timer timeout\n");
    gpio_set_level(BAD_STATUS_LED_PIN,0);
}

void motorTimeoutCallback(TimerHandle_t timerHandle){
    esp_rom_printf("Motor timeout timer called\n");
    gpio_set_level(MOTOR_DRIVER_IN1_PIN,0);
}


//Custom printf. Prepends a message with a prefix to simplify analysing output
static int printf2(char *formattedInput, ...)
{
    int result;
    va_list args;
    va_start(args, formattedInput);
    printf(ANSI_COLOR_BLUE "SmartLockCore: " ANSI_COLOR_WHITE);
    result = vprintf(formattedInput, args);
    va_end(args);
    return result;
}