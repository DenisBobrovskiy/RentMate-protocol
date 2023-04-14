#include <stdio.h>
#include "../client.h"
#include "smartLockCore.h"
#include "matrixKeyboard.h"
#include <memory.h>
#include "driver/dac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <string.h>

#define ANSI_COLOR_BLUE "\x1B[34m"
#define ANSI_COLOR_WHITE "\x1B[37m"

#define GOOD_STATUS_LED_TIMER_ID 2
#define BAD_STATUS_LED_TIMER_ID 3
#define MOTOR_ROTATION_TIMEOUT_TIMER_ID 4 // Called when motor was rotating for too long (Ideally it should turn off before the timer expires due to
// GPIO interrupt from hitting a limit switch in the mechanism)
#define OPEN_SWITCH_TIMER_ID 5
#define CLOSED_SWITCH_TIMER_ID 6
#define PROXIMITY_DETECTOR_TIMER_ID 7

#define GOOD_STATUS_LED_DELAY_MS 500 // How long the status LED should stay on
#define BAD_STATUS_LED_DELAY_MS 500  // How long the status LED should stay on
// #define BUZZER_DELAY 300 //How long to activate the buzzer for
#define MOTOR_ROTATION_TIMEOUT_MS 1000 // How long the motor should rotate before stopping (timeout), this is a fallback mechanism in case a limit switch doesn't work

// Switch delays
#define OPEN_SWITCH_TIMER_DELAY 500
#define CLOSED_SWITCH_TIMER_DELAY 500
#define PROXIMITY_DETECTOR_TIMER_DELAY 500

// Current lock passcode, needs to be initialized from settings files
uint8_t currentPasscode[MAX_PASSWORD_LEN] = {1, 2, 4, 4};
uint8_t currentPasscodeLen = 4;

uint8_t currentDoorState = 0; // State of proximity sensor to detect if door was closed. 0 - door open (sensor not active). 1 - door closed (sensor active)
uint8_t currentLockState = 0; // State of lock limit switches to detect if lock was closed. 0 - lock open . 1 - lock closed

// Timers
TimerHandle_t goodLedTimer;
TimerHandle_t badLedTimer;
TimerHandle_t motorTimeoutTimer;
// TimerHandle_t buzzerTimer;
TimerHandle_t openSwitchTimer;
TimerHandle_t closedSwitchTimer;
TimerHandle_t proximityDetectorTimer;

uint8_t motorStatus = 0; // 0 = motor not active; 1 = motor clockwise rotating; 2 = motor counterclockwise rotating

uint8_t isOpenSwitchTimerRunning = 0;
uint8_t isClosedSwitchTimerRunning = 0;

// Static function prototypes
static int printf2(char *formattedInput, ...);

void initLock()
{

    // Enable interrupts
    gpio_install_isr_service(0);

    // Initialize matrix keypad
    initKeypad();

    // Intialize timers
    goodLedTimer = xTimerCreate("goodLedTimer", pdMS_TO_TICKS(GOOD_STATUS_LED_DELAY_MS), pdFALSE, (void *)GOOD_STATUS_LED_TIMER_ID, goodLedTimeoutCallback);
    badLedTimer = xTimerCreate("badLedTimer", pdMS_TO_TICKS(BAD_STATUS_LED_DELAY_MS), pdFALSE, (void *)BAD_STATUS_LED_TIMER_ID, badLedTimeoutCallback);
    // buzzerTimer = xTimerCreate("buzzerTimer", pdMS_TO_TICKS(BUZZER_DELAY), pdFALSE, (void*)BAD_STATUS_LED_TIMER_ID, badLedTimeoutCallback);
    motorTimeoutTimer = xTimerCreate("motorTimeoutTimer", pdMS_TO_TICKS(MOTOR_ROTATION_TIMEOUT_MS), pdFALSE, (void *)MOTOR_ROTATION_TIMEOUT_TIMER_ID, motorTimeoutCallback);

    // Switch timers
    openSwitchTimer = xTimerCreate("openSwitchTimer", pdMS_TO_TICKS(OPEN_SWITCH_TIMER_DELAY), pdFALSE, (void *)OPEN_SWITCH_TIMER_ID, openSwitchTimerCallback);
    closedSwitchTimer = xTimerCreate("closedSwitchTimer", pdMS_TO_TICKS(CLOSED_SWITCH_TIMER_DELAY), pdFALSE, (void *)CLOSED_SWITCH_TIMER_ID, closedSwitchTimerCallback);
    proximityDetectorTimer = xTimerCreate("proximityDetectorTimer", pdMS_TO_TICKS(PROXIMITY_DETECTOR_TIMER_DELAY), pdFALSE, (void *)PROXIMITY_DETECTOR_TIMER_ID, proximityTimerCallback);

    //Init power LED pin
    gpio_pad_select_gpio(POWER_LED_PIN);
    gpio_set_direction(POWER_LED_PIN,GPIO_MODE_OUTPUT);
    gpio_set_level(POWER_LED_PIN,1);

    // Intialize LED pins
    gpio_pad_select_gpio(GOOD_STATUS_LED_PIN);
    gpio_pad_select_gpio(BAD_STATUS_LED_PIN);
    gpio_set_direction(GOOD_STATUS_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(BAD_STATUS_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(GOOD_STATUS_LED_PIN, 0);
    gpio_set_level(BAD_STATUS_LED_PIN, 0);

    // Initialize buzzer pin
    gpio_pad_select_gpio(ACTIVE_BUZZER_PIN);
    gpio_set_direction(ACTIVE_BUZZER_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(ACTIVE_BUZZER_PIN, 0);

    // Initialize proximity sensor
    gpio_pad_select_gpio(PROXIMITY_SENSOR_PIN);
    gpio_set_direction(PROXIMITY_SENSOR_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PROXIMITY_SENSOR_PIN, GPIO_PULLDOWN_ONLY);
    gpio_set_intr_type(PROXIMITY_SENSOR_PIN, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add(PROXIMITY_SENSOR_PIN, proximitySensorInterruptCallback, NULL);

    // Initialize lock state switches pins
    gpio_pad_select_gpio(SWITCH_LOCK_OPEN_PIN);
    gpio_pad_select_gpio(SWITCH_LOCK_CLOSED_PIN);
    gpio_set_direction(SWITCH_LOCK_OPEN_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(SWITCH_LOCK_CLOSED_PIN, GPIO_MODE_INPUT);
    gpio_set_intr_type(SWITCH_LOCK_OPEN_PIN, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(SWITCH_LOCK_CLOSED_PIN, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add(SWITCH_LOCK_OPEN_PIN, lockSwitchStateInterrupt, (void *)2);
    gpio_isr_handler_add(SWITCH_LOCK_CLOSED_PIN, lockSwitchStateInterrupt, (void *)1);

    // Initialize motor driver.
    gpio_pad_select_gpio(MOTOR_DRIVER_IN1_PIN);
    gpio_pad_select_gpio(MOTOR_DRIVER_IN2_PIN);
    gpio_set_direction(MOTOR_DRIVER_IN1_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(MOTOR_DRIVER_IN2_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(MOTOR_DRIVER_IN1_PIN, 0);
    gpio_set_level(MOTOR_DRIVER_IN2_PIN, 0);
}

// Function called when a passcode was entered from keypad. THIS RUNS INSIDE INTERRUPT SO NORMAL PRINTF CANT BE USED OR CRASH
void passcodeEntered(uint8_t *passcode, uint8_t passcodeLen)
{
    esp_rom_printf("Passcode entered: ");
    for (int i = 0; i < passcodeLen; i++)
    {
        esp_rom_printf("%d", passcode[i]);
    }
    esp_rom_printf("\n");
    for (int i = 0; i < passcodeLen; i++)
    {
        esp_rom_printf("%d", currentPasscode[i]);
    }
    esp_rom_printf("\n");

    if (passcodeLen == currentPasscodeLen)
    {
        if (memcmp(currentPasscode, passcode, passcodeLen) == 0)
        {
            // Password entered good
            passcodeCorrect();
        }
        else
        {
            // Bad password
            passcodeWrong();
        }
    }
    else
    {
        // Bad password
        passcodeWrong();
    }
}

// Call this when passcode entered is wrong. Flash RED Led and maybe beeper
void passcodeWrong()
{
    esp_rom_printf("Password entered is wrong\n");

    // Turn on LED
    gpio_set_level(BAD_STATUS_LED_PIN, 1);

    // Turn on buzzer
    gpio_set_level(ACTIVE_BUZZER_PIN, 1);

    // Start timer to turn off LED
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    if (xTimerStartFromISR(badLedTimer, higherPriorityTaskWoken) != pdPASS)
    {
        // Timer failed starting
        esp_rom_printf("Bad led timeout timer failed starting\n");
    }
}

// Password correct, blink green LED and turn the motor
void passcodeCorrect()
{
    esp_rom_printf("Password entered is correct\n");

    // Turn on buzzer
    gpio_set_level(ACTIVE_BUZZER_PIN, 1);

    // Turn on good status LED
    gpio_set_level(GOOD_STATUS_LED_PIN, 1);

    // Start timer to turn off LED
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    if (xTimerStartFromISR(goodLedTimer, higherPriorityTaskWoken) != pdPASS)
    {
        // Timer failed starting
        esp_rom_printf("Good led timeout timer failed starting\n");
    }

    rotateMotorClockwise();
}

void rotateMotorClockwise()
{
    esp_rom_printf("Rotating motor clockwise\n");
    gpio_set_level(MOTOR_DRIVER_IN1_PIN, 1);
    gpio_set_level(MOTOR_DRIVER_IN2_PIN, 0);
    motorStatus = 1;
    // Start timer to turn off motor driver
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    if (xTimerStartFromISR(motorTimeoutTimer, higherPriorityTaskWoken) != pdPASS)
    {
        // Timer failed starting
        esp_rom_printf("Motor timeout timer failed starting\n");
    }
}

void rotateMotorAnticlockwise()
{
    esp_rom_printf("Rotating motor anticlockwise\n");
    gpio_set_level(MOTOR_DRIVER_IN1_PIN, 0);
    gpio_set_level(MOTOR_DRIVER_IN2_PIN, 1);
    motorStatus = 2;
    // Start timer to turn off motor driver
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    if (xTimerStartFromISR(motorTimeoutTimer, higherPriorityTaskWoken) != pdPASS)
    {
        // Timer failed starting
        esp_rom_printf("Motor timeout timer failed starting\n");
    }
}

void goodLedTimeoutCallback(TimerHandle_t timerHandle)
{
    esp_rom_printf("LED GOOD timer timeout\n");
    gpio_set_level(GOOD_STATUS_LED_PIN, 0);
    gpio_set_level(ACTIVE_BUZZER_PIN, 0);
}

void badLedTimeoutCallback(TimerHandle_t timerHandle)
{
    esp_rom_printf("LED BAD timer timeout\n");
    gpio_set_level(BAD_STATUS_LED_PIN, 0);
    gpio_set_level(ACTIVE_BUZZER_PIN, 0);
}

void motorTimeoutCallback(TimerHandle_t timerHandle)
{
    esp_rom_printf("Motor timeout timer called\n");
    gpio_set_level(MOTOR_DRIVER_IN1_PIN, 0);
    gpio_set_level(MOTOR_DRIVER_IN2_PIN, 0);
    motorStatus = 0;
}

// Set saveToMemory to 1 to save to memory and to 0 to not save.
int setPasscode(uint8_t passcode[MAX_PASSWORD_LEN], uint8_t passcodeLen, uint8_t saveToMemory)
{
    printf2("Setting new passcode\n");

    memcpy(currentPasscode, passcode, passcodeLen);
    currentPasscodeLen = passcodeLen;

    // Print new passcode after settings it successfully
    printf2("New passcode set: ");
    for (int i = 0; i < passcodeLen; i++)
    {
        printf("%d", currentPasscode[i]);
    }
    printf("\n");

    // Save to memory if needed
    if (saveToMemory == 1)
    {
        nvs_handle handle;
        esp_err_t err = nvs_open("nvs", NVS_READWRITE, &handle);
        if (err != ESP_OK)
        {
            printf2("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        }
        else
        {
            printf2("Opened NVS storage successfully. Proceeding to save the new passcode\n");
        }

        err = nvs_set_u8(handle, "passcodeLen", currentPasscodeLen);
        if (err == ESP_OK)
        {
            printf2("Saved passcode length successfully\n");
        }
        else
        {
            const char *errorName = esp_err_to_name(err);
            printf2("Failed saving passcode length. Error: %s\n", errorName);
        }
        err = nvs_set_blob(handle, "passcode", currentPasscode, currentPasscodeLen);
        if (err == ESP_OK)
        {
            printf2("Saved passcode successfully\n");
        }
        else
        {
            const char *errorName = esp_err_to_name(err);
            printf2("Failed saving passcode. Error: %s\n", errorName);
        }

        nvs_close(handle);
    }

    return 1;
}

void IRAM_ATTR lockSwitchStateInterrupt(void *args)
{
    // If args = 1 - lock closed switch activated
    // If args = 2 - lock opened switch activated
    int switchType = (int)args;

    BaseType_t higherPriorityTaskWoken = pdFALSE;
    if (switchType == 1)
    {
        // esp_rom_printf("Lock closed switch activated\n");
        if (isClosedSwitchTimerRunning == 0)
        {
            if (xTimerStartFromISR(closedSwitchTimer, higherPriorityTaskWoken) != pdPASS)
            {
                // Timer failed starting
                esp_rom_printf("Opened switch timer failed starting\n");
            }
            else
            {
                isClosedSwitchTimerRunning = 1;
            }
        }
    }
    else if (switchType == 2)
    {
        // esp_rom_printf("Lock opened switch activated\n");
        if (isOpenSwitchTimerRunning == 0)
        {
            if (xTimerStartFromISR(openSwitchTimer, higherPriorityTaskWoken) != pdPASS)
            {
                // Timer failed starting
                esp_rom_printf("Closed switch timer failed starting\n");
            }
            else
            {
                isOpenSwitchTimerRunning = 1;
            }
        }
    }
}

void openSwitchTimerCallback(TimerHandle_t timerHandle)
{
    // After the switch has debounced check the final state
    // esp_rom_printf("open switch timer finished\n");
    int output = 0;
    output = gpio_get_level(SWITCH_LOCK_OPEN_PIN);

    if (output == 0)
    {
        // This means the switch was activated, lock entered open state
        esp_rom_printf("LOCK OPENED\n");
        currentLockState = 0;
    }
    isOpenSwitchTimerRunning = 0;
}

void closedSwitchTimerCallback(TimerHandle_t timerHandle)
{
    // After the switch has debounced check the final state
    // esp_rom_printf("Closed switch timer finished\n");
    int output = 0;
    output = gpio_get_level(SWITCH_LOCK_CLOSED_PIN);

    if (output == 0)
    {
        // This means the switch was activated, lock entered closed state
        esp_rom_printf("LOCK CLOSED\n");
        currentLockState = 1;
    }
    isClosedSwitchTimerRunning = 0;
}

void IRAM_ATTR proximitySensorInterruptCallback(void *args)
{
    esp_rom_printf("Proximity sensor callback called\n");

    // This starts or restarts the proximity sensor timer on positive and negative edge signals
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    if (xTimerResetFromISR(proximityDetectorTimer, higherPriorityTaskWoken) != pdPASS)
    {
        // Timer failed starting
        esp_rom_printf("Proximity timer failed starting/restarting\n");
    }
}

void proximityTimerCallback(TimerHandle_t timerHandle)
{
    int output = 0;
    output = gpio_get_level(PROXIMITY_SENSOR_PIN);

    if (output == 0)
    {
        // Means proximity sensor was deactivated
        esp_rom_printf("DOOR OPENED\n");
        currentDoorState = 0;
    }
    else if (output == 1)
    {
        // Proximity sensor was activated
        esp_rom_printf("DOOR CLOSED\n");
        currentDoorState = 1;
    }
}

// Custom printf. Prepends a message with a prefix to simplify analysing output
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