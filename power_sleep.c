#include "power_sleep.h"

#include <stdbool.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/rtc.h"

static volatile bool rtc_fired = false;

static void rtc_callback(void)
{
    rtc_fired = true;
}

void power_sleep_for_seconds(uint32_t seconds)
{
    datetime_t t = {
        .year = 2025,
        .month = 1,
        .day = 1,
        .dotw = 3,
        .hour = 0,
        .min = 0,
        .sec = 0,
    };

    rtc_init();
    rtc_set_datetime(&t);
    sleep_us(64);

    datetime_t alarm = {
        .year = 2025,
        .month = 1,
        .day = 1,
        .dotw = 3,
        .hour = (int8_t)(seconds / 3600),
        .min = (int8_t)((seconds % 3600) / 60),
        .sec = (int8_t)(seconds % 60),
    };

    rtc_fired = false;
    rtc_set_alarm(&alarm, rtc_callback);

    printf("Sleeping for %lu seconds...\n", (unsigned long)seconds);
    uart_default_tx_wait_blocking();

    while (!rtc_fired)
    {
        __asm volatile("wfi");
    }
}
