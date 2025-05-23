/**
**************************************************************
* @file DT1/src/rtc.h
* @author Travis Graham - 47437553
* @date 28/03/2025
* @brief RTC driver header file
***************************************************************
* EXTERNAL FUNCTIONS
***************************************************************
* void init_rtc(void); - init the real time counter
* uint32_t get_rtc_time(); - get the current set time
***************************************************************
*/
#ifndef RTC_H
#define RTC_H

#include <zephyr/types.h>

#define CMD_PARAM_FORMATTED "r" 

/* Function Prototypes */
void init_rtc(void);
uint32_t get_rtc_time();
char* get_rtc_time_formatted();
void set_rtc_time(int seconds, int minutes, int hours);

extern uint32_t offset_system_seconds;
#endif
