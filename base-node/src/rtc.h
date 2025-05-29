#ifndef RTC_H
#define RTC_H
#include <zephyr/drivers/rtc.h>

void init_rtc(void);
void set_rtc_time(struct rtc_time *time);
void get_rtc_time(struct rtc_time *time);

#endif // RTC_H