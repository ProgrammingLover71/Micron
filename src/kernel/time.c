#include "time.h"
#include "../io/io.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_RELOAD   65536u
#define PIT_HZ       1193182u

static uint8_t cmos_read(uint8_t reg)
{
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static int rtc_updating(void)
{
    return (cmos_read(0x0A) & 0x80) != 0;
}

static uint8_t bcd_to_bin(uint8_t value)
{
    return (uint8_t)((value & 0x0F) + ((value >> 4) * 10));
}

static uint16_t pit_read_counter(void)
{
    uint8_t lo;
    uint8_t hi;

    outb(PIT_COMMAND, 0x00);
    lo = inb(PIT_CHANNEL0);
    hi = inb(PIT_CHANNEL0);

    return (uint16_t)lo | ((uint16_t)hi << 8);
}

void time_sleep_ms(uint32_t ms)
{
    uint16_t prev = pit_read_counter();
    uint32_t elapsed = 0;
    uint32_t target = ms * (PIT_HZ / 1000u);

    while (elapsed < target) {
        uint16_t now = pit_read_counter();
        if (prev >= now)
            elapsed += (uint32_t)(prev - now);
        else
            elapsed += (uint32_t)prev + (PIT_RELOAD - now);
        prev = now;
        __asm__ volatile ("pause");
    }
}

void sleep(uint32_t seconds)
{
    while (seconds-- > 0)
        time_sleep_ms(1000);
}

int time_read_rtc(rtc_time_t *time)
{
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t status_b;

    if (!time)
        return 0;

    while (rtc_updating())
        ;

    second = cmos_read(0x00);
    minute = cmos_read(0x02);
    hour = cmos_read(0x04);
    day = cmos_read(0x07);
    month = cmos_read(0x08);
    year = cmos_read(0x09);
    status_b = cmos_read(0x0B);

    if ((status_b & 0x04) == 0) {
        second = bcd_to_bin(second);
        minute = bcd_to_bin(minute);
        hour = (uint8_t)((hour & 0x80) | bcd_to_bin((uint8_t)(hour & 0x7F)));
        day = bcd_to_bin(day);
        month = bcd_to_bin(month);
        year = bcd_to_bin(year);
    }

    if ((status_b & 0x02) == 0 && (hour & 0x80)) {
        hour = (uint8_t)(((hour & 0x7F) + 12) % 24);
    }

    time->year = (uint16_t)(2000 + year);
    time->month = month;
    time->day = day;
    time->hour = hour;
    time->minute = minute;
    time->second = second;
    return 1;
}
