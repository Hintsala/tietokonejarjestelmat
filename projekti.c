#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <pico/stdlib.h>
#include <FreeRTOS.h>
#include <task.h>
#include "tkjhat/sdk.h"

#define DEFAULT_STACK_SIZE 2048
#define SENSOR_DELAY_MS    50
#define PRINT_DELAY_MS     50
#define THRESHOLD_G        0.8f

#define SW1_FLAG (1 << 0)
#define SW2_FLAG (1 << 1)

typedef enum {
    WAITING,
    BUTTON_PRESSED,
    SEND_SYMBOL
} state_t;

typedef struct {
    float ax;
    float ay;
    float az;
} accel_t;

volatile state_t programState = WAITING;
volatile uint8_t buttonFlags = 0;
volatile char symbol = 0;

static void btn_fxn(uint gpio, uint32_t eventMask) {
    if (programState != WAITING) {
        return;
    }
    if (gpio == BUTTON1) {
        buttonFlags |= SW1_FLAG;
    } else if (gpio == BUTTON2) {
        buttonFlags |= SW2_FLAG;
    }
    programState = BUTTON_PRESSED;
}

static void read_accel(accel_t *data) {
    float gx, gy, gz, t;
    ICM42670_read_sensor_data(&data->ax, &data->ay, &data->az, &gx, &gy, &gz, &t);
}

static char detect_symbol(const accel_t *data) {
    if (fabsf(data->az) > THRESHOLD_G) {
        return '.';
    }
    if (fabsf(data->ax) > THRESHOLD_G || fabsf(data->ay) > THRESHOLD_G) {
        return '-';
    }
    return 0;
}

static void sensor_task(void *arg) {
    accel_t accel;

    init_ICM42670();
    ICM42670_start_with_default_values();

    while (1) {
        if (programState == BUTTON_PRESSED) {
            if (buttonFlags & SW1_FLAG) {
                read_accel(&accel);
                symbol = detect_symbol(&accel);
            } else if (buttonFlags & SW2_FLAG) {
                symbol = ' ';
            }

            buttonFlags = 0;

            if (symbol != 0) {
                programState = SEND_SYMBOL;
            } else {
                programState = WAITING;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(SENSOR_DELAY_MS));
    }
}

static void print_task(void *arg) {
    while (1) {
        if (programState == SEND_SYMBOL) {
            printf("%c", symbol);
            fflush(stdout);
            toggle_led();
            symbol = 0;
            programState = WAITING;
        }
        vTaskDelay(pdMS_TO_TICKS(PRINT_DELAY_MS));
    }
}

int main(void) {
    stdio_init_all();
    init_hat_sdk();
    sleep_ms(300);

    init_led();
    init_sw1();
    init_sw2();

    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_RISE, true, btn_fxn);
    gpio_set_irq_enabled(BUTTON2, GPIO_IRQ_EDGE_RISE, true);

    xTaskCreate(sensor_task, "sensor", DEFAULT_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(print_task, "print", DEFAULT_STACK_SIZE, NULL, 2, NULL);

    vTaskStartScheduler();

    return 0;
}