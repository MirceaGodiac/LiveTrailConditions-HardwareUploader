#include <stdio.h>

#include "pico/stdlib.h"

#include "app_config.h"
#include "modem.h"
#include "power_sleep.h"
#include "sensors.h"
#include "upload_cycle.h"

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(void)
{
    stdio_init_all();
    setvbuf(stdout, NULL, _IONBF, 0);
    sleep_ms(3000);
    printf("Booting Live Trail uploader (%s %s)...\n", __DATE__, __TIME__);

    sensors_init();
    modem_init();

    // Brief stabilization wait on first boot only.
    sleep_ms(5000);
    printf("Ready.\n");

    while (true)
    {
        printf("Starting upload cycle...\n");
        upload_cycle_run();

        printf("Entering dormant sleep...\n");
        power_sleep_for_seconds(SLEEP_SECONDS);
        printf("Woke up.\n");
    }

    return 0;
}