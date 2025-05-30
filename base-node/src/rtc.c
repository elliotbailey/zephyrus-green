#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/rtc.h>

#include "rtc.h"

#define RTC_NODE DT_NODELABEL(rtc)

LOG_MODULE_REGISTER(rtc_module, LOG_LEVEL_DBG);

const struct device *rtc_dev;

// setup RTC mutex
K_MUTEX_DEFINE(rtc_mutex);

/**
 * Set the RTC time
 * 
 * @param time Pointer to the rtc_time structure containing the time to set
 */
void set_rtc_time(struct rtc_time *time) {
    k_mutex_lock(&rtc_mutex, K_FOREVER);
    rtc_set_time(rtc_dev, time);
    k_mutex_unlock(&rtc_mutex);
    LOG_INF("RTC time set to: %04d-%02d-%02d %02d:%02d:%02d\n",
        time->tm_year + 1900,
        time->tm_mon + 1,
        time->tm_mday,
        time->tm_hour,
        time->tm_min,
        time->tm_sec
    );
}

/**
 * Get the current RTC time
 * 
 * @param time Pointer to the rtc_time structure to store the current time
 */
void get_rtc_time(struct rtc_time *time) {
    k_mutex_lock(&rtc_mutex, K_FOREVER);
    rtc_get_time(rtc_dev, time);
    k_mutex_unlock(&rtc_mutex);
}

/**
 * Initialise the RTC device and set an initial time
 */
void init_rtc(void) {
    // init RTC device
    rtc_dev = DEVICE_DT_GET(RTC_NODE);
    // set initial time
    struct rtc_time set_time = {
        .tm_year = 2025 - 1900,  // Year since 1900
        .tm_mon = 5 - 1,         // Month (0-based)
        .tm_mday = 22,           // Day
        .tm_hour = 0,           // Hour
        .tm_min = 0,            // Minute
        .tm_sec = 0              // Second
    };
    set_rtc_time(&set_time);
    LOG_DBG("RTC initialised");
}


void format_rtc_time(char *buf, size_t maxlen) {
    struct rtc_time time;
    get_rtc_time(&time);
    snprintf(buf, maxlen, "%04d-%02d-%02d %02d:%02d:%02d",
        time.tm_year + 1900,
        time.tm_mon + 1,
        time.tm_mday,
        time.tm_hour,
        time.tm_min,
        time.tm_sec
    );
}
