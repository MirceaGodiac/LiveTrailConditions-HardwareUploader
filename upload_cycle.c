#include "upload_cycle.h"

#include <stdio.h>

#include "app_config.h"
#include "modem.h"
#include "sensors.h"

void upload_cycle_run(void)
{
    char json[128];

    int moisture_pct = sensors_read_moisture_percent();
    int battery_pct = sensors_read_battery_percent();

    if (!modem_connect_network())
    {
        printf("Upload skipped: modem/network unavailable.\n");
        return;
    }

    snprintf(json, sizeof(json),
             "{\"trailId\":\"%s\",\"moisture\":%d,\"battery\":%d}",
             APP_TRAIL_ID,
             moisture_pct,
             battery_pct);

    printf("Sending JSON: %s\n", json);

    if (!modem_http_post_json(APP_API_URL, APP_API_KEY, json))
    {
        printf("HTTP POST may have failed.\n");
    }

    modem_shutdown_for_sleep();
}
