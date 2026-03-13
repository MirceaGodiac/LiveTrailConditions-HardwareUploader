#include "sensors.h"

#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "app_config.h"
#include "utils.h"

static uint16_t read_adc_gpio(uint gpio)
{
    adc_select_input(gpio - 26);
    sleep_us(10);
    return adc_read();
}

static float read_battery_voltage(void)
{
    uint16_t raw = read_adc_gpio(BATTERY_PIN);
    return (float)raw * (3.3f / 4095.0f) * 3.0f;
}

void sensors_init(void)
{
    gpio_init(SENSOR_POWER_PIN);
    gpio_set_dir(SENSOR_POWER_PIN, GPIO_OUT);
    gpio_put(SENSOR_POWER_PIN, 0);

    adc_init();
    adc_gpio_init(MOISTURE_PIN);
    adc_gpio_init(BATTERY_PIN);
}

int sensors_read_moisture_percent(void)
{
    gpio_put(SENSOR_POWER_PIN, 1);
    sleep_ms(500);

    int moisture_raw = (int)read_adc_gpio(MOISTURE_PIN);

    gpio_put(SENSOR_POWER_PIN, 0);

    return clamp_int(map_int(moisture_raw, AIR_VALUE, WATER_VALUE, 0, 100), 0, 100);
}

int sensors_read_battery_percent(void)
{
    float voltage = read_battery_voltage();
    int centivolts = (int)(voltage * 100.0f);
    int percentage = map_int(centivolts, 1110, 1260, 0, 100);
    return clamp_int(percentage, 0, 100);
}
