#ifndef RTC_H
#define RTC_H
#include <zephyr/drivers/rtc.h>

void init_rtc(void);
void set_rtc_time(struct rtc_time *time);
void get_rtc_time(struct rtc_time *time);
void format_rtc_time(char *buf, size_t maxlen);

#endif // RTC_H