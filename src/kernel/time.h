#pragma once

#include <stdint.h>

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} rtc_time_t;

void sleep(uint32_t seconds);
void time_sleep_ms(uint32_t ms);
int time_read_rtc(rtc_time_t *time);
