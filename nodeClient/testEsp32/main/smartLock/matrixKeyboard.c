
#include <stdio.h>
#include "../client.h"
#include "matrixKeyboard.h"
#include "esp_log.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "smartLock/smartLockCore.h"

TimerHandle_t debounceTimer;
SemaphoreHandle_t debounceTimerSemaphore;
int isDebounceTimerRunning = 0;

uint8_t passwordBuffer[MAX_PASSWORD_LEN];
uint8_t passwordCurrentLength = 0;

void initKeypad()
{
    gpio_pad_select_gpio(2);
    gpio_set_direction(2, GPIO_MODE_OUTPUT);

    // Define keypad pins as gpio
    gpio_pad_select_gpio(KEYPAD_ROW1_PIN); // Row 1
    gpio_pad_select_gpio(KEYPAD_ROW2_PIN); // Row 2
    gpio_pad_select_gpio(KEYPAD_ROW3_PIN); // Row 3
    gpio_pad_select_gpio(KEYPAD_ROW4_PIN); // Row 4
    gpio_pad_select_gpio(KEYPAD_COL1_PIN); // Col 1
    gpio_pad_select_gpio(KEYPAD_COL2_PIN); // Col 2
    gpio_pad_select_gpio(KEYPAD_COL3_PIN); // Col 3

    // Set outputs/inputs for keypad
    gpio_set_direction(KEYPAD_ROW1_PIN, GPIO_MODE_INPUT);  // Row 1
    gpio_set_direction(KEYPAD_ROW2_PIN, GPIO_MODE_INPUT);  // Row 2
    gpio_set_direction(KEYPAD_ROW3_PIN, GPIO_MODE_INPUT);  // Row 3
    gpio_set_direction(KEYPAD_ROW4_PIN, GPIO_MODE_INPUT);  // Row 4
    gpio_set_direction(KEYPAD_COL1_PIN, GPIO_MODE_OUTPUT); // Col 1
    gpio_set_direction(KEYPAD_COL2_PIN, GPIO_MODE_OUTPUT); // Col 2
    gpio_set_direction(KEYPAD_COL3_PIN, GPIO_MODE_OUTPUT); // Col 3

    // Set all row pins (inputs) to have a pull up resistor
    gpio_set_pull_mode(KEYPAD_ROW1_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(KEYPAD_ROW2_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(KEYPAD_ROW3_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(KEYPAD_ROW4_PIN, GPIO_PULLUP_ONLY);

    // Set all column pins to output 0 volts
    gpio_set_level(KEYPAD_COL1_PIN, 0);
    gpio_set_level(KEYPAD_COL2_PIN, 0);
    gpio_set_level(KEYPAD_COL3_PIN, 0);

    // Set interrupts on row pins(inputs)

    gpio_set_intr_type(KEYPAD_ROW1_PIN, GPIO_INTR_NEGEDGE);
    gpio_set_intr_type(KEYPAD_ROW2_PIN, GPIO_INTR_NEGEDGE);
    gpio_set_intr_type(KEYPAD_ROW3_PIN, GPIO_INTR_NEGEDGE);
    gpio_set_intr_type(KEYPAD_ROW4_PIN, GPIO_INTR_NEGEDGE);


    gpio_isr_handler_add(KEYPAD_ROW1_PIN, keypadInterruptHandler, (void *)1);
    gpio_isr_handler_add(KEYPAD_ROW2_PIN, keypadInterruptHandler, (void *)2);
    gpio_isr_handler_add(KEYPAD_ROW3_PIN, keypadInterruptHandler, (void *)3);
    gpio_isr_handler_add(KEYPAD_ROW4_PIN, keypadInterruptHandler, (void *)4);

    // Initialize a debounce timer
    debounceTimer = xTimerCreate("debounceTimer", pdMS_TO_TICKS(KEYPAD_DEBOUNCE_MS), pdFALSE, (void*)1, debounceTimerCallback);
    debounceTimerSemaphore = xSemaphoreCreateBinary();
}

void debounceTimerCallback(TimerHandle_t handle)
{
    esp_rom_printf("Timer interrupt called\n");
    // BaseType_t higherPriorityTaskWoken = pdFALSE;
    // if(xSemaphoreTakeFromISR(debounceTimerSemaphore,&higherPriorityTaskWoken)!=pdTRUE){
    //     //Semaphore couldnt be taken, restart the timer
    // }else{
    //     isDebounceTimerRunning = 0;
    //     xSemaphoreGiveFromISR(debounceTimerSemaphore,&higherPriorityTaskWoken);
    // }
    isDebounceTimerRunning = 0;
}

void IRAM_ATTR keypadInterruptHandler(void *args)
{
    uint32_t rowNum = (uint32_t)args;
    // printf("Fired a key press interrupt for keypad row: %d\n",rowNum);
    // printf("Interrupt\n");

    BaseType_t higherPriorityTaskWoken = pdFALSE;
    gpio_set_level(2, 1);
    esp_rom_printf("KeypadInterrupt: Interrupt called for row: %d\n", rowNum);

    // Check if debounce timer is running
    if (isDebounceTimerRunning == 1)
    {
        // Timer running, exit;
        return;
    }
    // xSemaphoreTake(debounceTimerSemaphore,100);
    // xSemaphoreGive(debounceTimerSemaphore);
    isDebounceTimerRunning = 1;
    esp_rom_printf("Registered key press!\n");
    if (xTimerStartFromISR(debounceTimer, &higherPriorityTaskWoken) != pdPASS)
    {
        esp_rom_printf("KeypadInterrupt: Failed starting timer\n");
    }

    int finalColVal = 0;

    int rowPinNum = 0;
    if (rowNum == 1)
    {
        rowPinNum = KEYPAD_ROW1_PIN;
    }
    else if (rowNum == 2)
    {
        rowPinNum = KEYPAD_ROW2_PIN;
    }
    else if (rowNum == 3)
    {
        rowPinNum = KEYPAD_ROW3_PIN;
    }
    else if (rowNum == 4)
    {
        rowPinNum = KEYPAD_ROW4_PIN;
    }
    // Now figure out which column it is on the keypad
    gpio_set_level(KEYPAD_COL1_PIN, 1);
    gpio_set_level(KEYPAD_COL2_PIN, 0);
    gpio_set_level(KEYPAD_COL3_PIN, 0);
    int colNum1 = gpio_get_level(rowPinNum);

    gpio_set_level(KEYPAD_COL1_PIN, 0);
    gpio_set_level(KEYPAD_COL2_PIN, 1);
    gpio_set_level(KEYPAD_COL3_PIN, 0);
    int colNum2 = gpio_get_level(rowPinNum);

    gpio_set_level(KEYPAD_COL1_PIN, 0);
    gpio_set_level(KEYPAD_COL2_PIN, 0);
    gpio_set_level(KEYPAD_COL3_PIN, 1);
    int colNum3 = gpio_get_level(rowPinNum);

    if (colNum1 == 1)
    {
        finalColVal = 1;
    }
    else if (colNum2 == 1)
    {
        finalColVal = 2;
    }
    else if (colNum3 == 1)
    {
        finalColVal = 3;
    }
    esp_rom_printf("Final col value: %d\n", finalColVal);

    // Get the final value of keypad press
    uint8_t keyVal = 20; // Normal number codes for numbers. * is delete code (10). # is submit code (11).
    if (rowNum == 1)
    {
        if (finalColVal == 1)
        {
            keyVal = 1;
        }
        else if (finalColVal == 2)
        {
            keyVal = 2;
        }
        else if (finalColVal == 3)
        {
            keyVal = 3;
        }
    }
    else if (rowNum == 2)
    {
        if (finalColVal == 1)
        {
            keyVal = 4;
        }
        else if (finalColVal == 2)
        {
            keyVal = 5;
        }
        else if (finalColVal == 3)
        {
            keyVal = 6;
        }
    }
    else if (rowNum == 3)
    {
        if (finalColVal == 1)
        {
            keyVal = 7;
        }
        else if (finalColVal == 2)
        {
            keyVal = 8;
        }
        else if (finalColVal == 3)
        {
            keyVal = 9;
        }
    }
    else if (rowNum == 4)
    {
        if (finalColVal == 1)
        {
            keyVal = 10;
        }
        else if (finalColVal == 2)
        {
            keyVal = 11;
        }
        else if (finalColVal == 3)
        {
            keyVal = 11;
        }
    }

    esp_rom_printf("Final key value: %d\n", keyVal);

    // Send the letter to the password buffer
    if (keyVal != 20)
    {
        if (keyVal == 10)
        {
            // Check if we clicked delete button
            if (passwordCurrentLength > 0)
            {
                passwordCurrentLength--;
            }
        }
        else if (keyVal == 11)
        {
            // Check is we clicked submit button
             passcodeEntered(passwordBuffer,passwordCurrentLength);
             passwordCurrentLength = 0;
        }
        else if (passwordCurrentLength < MAX_PASSWORD_LEN)
        {
            // Add another letter if not over the limit of the buffer
            passwordBuffer[passwordCurrentLength] = keyVal;
            passwordCurrentLength++;
        }
        else
        {
            // Password too long, submit what we have
             passcodeEntered(passwordBuffer,passwordCurrentLength);
             passwordCurrentLength = 0;
        }
    }
}