
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/timer/system_timer.h>
#include <zephyr/sys/time_units.h>
#include <stdlib.h>
#include <math.h>
#include "ultrasonic_handler.h"


//static const struct device *trig_port = DEVICE_DT_GET(DT_NODELABEL(gpio0)); // ESP32
//static const struct device *echo_port = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static const struct device *trig_port = DEVICE_DT_GET(DT_NODELABEL(gpiob)); // Nucleo (For sampling)
static const struct device *echo_port = DEVICE_DT_GET(DT_NODELABEL(gpiob));
static const struct device *mqtt_port = DEVICE_DT_GET(DT_NODELABEL(gpioa));
static struct gpio_callback pulse_info;

uint32_t pulse_start_time;
uint32_t pulse_duration;
uint8_t pulse_active = 0;

static void pulse_callback(const struct device *dev, struct gpio_callback *callback, uint32_t pin) {
    if (pulse_active) {
        pulse_duration = k_cyc_to_us_floor32(k_cycle_get_32()) - pulse_start_time;
        pulse_active = 0;
    } else {
        pulse_start_time = k_cyc_to_us_floor32(k_cycle_get_32());
        //printf("Pulse started\n");
        pulse_active = 1;
    }
}

void ultrasonic_init(void) {
    //gpio_pin_configure(trig_port, 5, GPIO_OUTPUT);
    //gpio_pin_configure(echo_port, 4, GPIO_INPUT);
    //gpio_pin_interrupt_configure(echo_port, 4, GPIO_INT_EDGE_BOTH);
    //gpio_init_callback(&pulse_info, pulse_callback, 1 << 4);
    gpio_pin_configure(trig_port, 9, GPIO_OUTPUT);
    gpio_pin_configure(echo_port, 8, GPIO_INPUT);
    gpio_pin_interrupt_configure(echo_port, 8, GPIO_INT_EDGE_BOTH);
    gpio_init_callback(&pulse_info, pulse_callback, 1 << 8);
    gpio_add_callback(echo_port, &pulse_info);
    gpio_pin_configure(mqtt_port, 5, GPIO_OUTPUT);
    gpio_pin_configure(mqtt_port, 6, GPIO_OUTPUT);
    gpio_pin_configure(mqtt_port, 7, GPIO_OUTPUT);
    gpio_pin_set(mqtt_port, 5, 0);
    gpio_pin_set(mqtt_port, 6, 0);
    gpio_pin_set(mqtt_port, 7, 0);
}

float ultrasonic_read(void) {
    //gpio_pin_set(trig_port, 5, 1);
    gpio_pin_set(trig_port, 9, 1);
    k_busy_wait(10);
    //gpio_pin_set(trig_port, 5, 0);
    gpio_pin_set(trig_port, 9, 0);
    k_msleep(50);
    // Speed of sound assumed
    //printf("Duration=%d\n", pulse_duration);
    return fmin(((pulse_duration / 1000000.0f) * 343.0f / 2.0f), 3.00);
}

void ultrasonic_publish(int category) {
    //printf("Category %d\n", category);
    
    if (category == 1) {
        gpio_pin_toggle(mqtt_port, 5);
        //int pin_state = gpio_pin_get_raw(mqtt_port, 5);
        //printf("Pin state before wait: %d\n", pin_state);
        k_msleep(500);
    } else if (category == 2) {
        gpio_pin_toggle(mqtt_port, 6);
        k_msleep(500);
    } else if (category == 3) {
        //gpio_pin_toggle(mqtt_port, 7);
        //k_msleep(500);
    }
    
    //gpio_pin_toggle(mqtt_port, 5);
    k_msleep(500);
}
