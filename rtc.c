/**
**************************************************************
* @file DT1/src/rtc.c
* @author Travis Graham - 47437553
* @date 28/03/2025
* @brief RTC driver
***************************************************************
* EXTERNAL FUNCTIONS
***************************************************************

***************************************************************
*/

#include <zephyr/kernel.h>
#include <stddef.h> 
#include <zephyr/types.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>

#include <zephyr/drivers/counter.h>
#include <zephyr/shell/shell.h>

#include "../include/rtc.h"

/* Get the RTC node from Devicetree */
#define RTC_NODE DT_NODELABEL(rtc0)

/* Enable shell debug output */
LOG_MODULE_REGISTER(led_module, LOG_LEVEL_DBG);

/* Define a static device pointer */
static const struct device *rtc0;

/* Set time to offset by */
uint32_t offset_system_seconds = 0;

void init_rtc(void) {
  rtc0 = DEVICE_DT_GET(RTC_NODE);  // Initialise the hardware
  if (!device_is_ready(rtc0)) {
      printf("RTC device is not ready\r\n");
  }
  int err = counter_start(rtc0);
  if (err) {
    printf("Failed to start real time counter!\r\n");
  }
}

/** 
* Returns the current counter value in seconds
* @param void 
* @returns ticks a float representation of the real time seconds
*/
uint32_t get_rtc_time() {
  uint32_t ticks;
  uint32_t freq = counter_get_frequency(rtc0); 
  counter_get_value(rtc0, &ticks);

  uint32_t real_time = ticks / freq + offset_system_seconds; // offset by set time

  return real_time;  // Ticks in seconds
}

/**
 * Returns a formatted time string in the format HH:MM:SS
 * @param void
 * @returns Pointer to static time string
 */
 char* get_rtc_time_formatted() {
  static char time_str[9]; // Buffer for "HH:MM:SS\0"
  uint32_t time_seconds = get_rtc_time();
  
  // Calculate hours, minutes, seconds
  uint32_t hours = (time_seconds / 3600) % 24;
  uint32_t minutes = (time_seconds / 60) % 60;
  uint32_t seconds = time_seconds % 60;
  
  // Format the time string
  snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", hours, minutes, seconds);
  
  return time_str;
}

/** 
 Set the offset time ticks for real time
*/
void set_rtc_time(int seconds, int minutes, int hours) {
  offset_system_seconds = (seconds) + (minutes * 60) + (hours * 60 * 60);
}
