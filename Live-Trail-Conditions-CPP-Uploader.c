#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/uart.h"

// --- User Configuration ---
static const char *SIM_PIN = "1408";
static const char *TRAIL_ID = "2";
static const char *API_URL = "https://live-trail-server.vercel.app/api/data";
static const char *API_KEY = "a628d17f-fdae-4d1b-9616-51328a75a0a0359fd79a338f4b2c8eed567f8642fbbf";

// --- Pin Definitions (Raspberry Pi Pico / RP2040) ---
#define MODEM_TX_PIN 1
#define MODEM_RX_PIN 0
#define MOISTURE_PIN 26
#define BATTERY_PIN 27
#define SENSOR_POWER_PIN 22
#define MODEM_PWRKEY_PIN 3

// --- Calibration Constants ---
static const int AIR_VALUE = 500;
static const int WATER_VALUE = 200;

// 6 hours in milliseconds.
static const uint32_t SLEEP_DURATION_MS = 21600000;

#define MODEM_UART uart0
#define MODEM_BAUDRATE 115200

static void sleep_with_progress(uint32_t total_ms, uint32_t chunk_ms, const char *label)
{
    uint32_t elapsed = 0;
    while (elapsed < total_ms)
    {
        uint32_t remaining = total_ms - elapsed;
        uint32_t step = (remaining < chunk_ms) ? remaining : chunk_ms;
        sleep_ms(step);
        elapsed += step;

        printf("%s: %lu/%lu s\n", label,
               (unsigned long)(elapsed / 1000),
               (unsigned long)(total_ms / 1000));
    }
}

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

static int map_int(int x, int in_min, int in_max, int out_min, int out_max)
{
    if (in_max == in_min)
    {
        return out_min;
    }
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static void modem_flush_rx(void)
{
    while (uart_is_readable(MODEM_UART))
    {
        (void)uart_getc(MODEM_UART);
    }
}

static void modem_send_line(const char *line)
{
    uart_puts(MODEM_UART, line);
    uart_puts(MODEM_UART, "\r\n");
}

static void send_at_command(const char *command, uint32_t timeout_ms, char *response, size_t response_size)
{
    size_t idx = 0;

    if (response_size > 0)
    {
        response[0] = '\0';
    }

    modem_flush_rx();
    modem_send_line(command);

    absolute_time_t start = get_absolute_time();
    while ((uint64_t)absolute_time_diff_us(start, get_absolute_time()) < (uint64_t)timeout_ms * 1000ULL)
    {
        while (uart_is_readable(MODEM_UART))
        {
            char c = (char)uart_getc(MODEM_UART);
            if (idx + 1 < response_size)
            {
                response[idx++] = c;
                response[idx] = '\0';
            }
        }
        sleep_ms(1);
    }

    if (idx > 0)
    {
        printf("CMD: %s\n%s\n", command, response);
    }
}

static void toggle_modem_power(void)
{
    printf("Toggling modem power key...\n");
    gpio_put(MODEM_PWRKEY_PIN, 0);
    sleep_ms(1500);
    gpio_put(MODEM_PWRKEY_PIN, 1);
    sleep_ms(3000);
}

static void shutdown_modem(void)
{
    char response[1024];

    printf("Shutting down modem...\n");
    send_at_command("AT+CPOWD=1", 5000, response, sizeof(response));
    if (strstr(response, "NORMAL POWER DOWN") != NULL || strstr(response, "OK") != NULL)
    {
        return;
    }

    // Fallback used by many Quectel modules.
    send_at_command("AT+QPOWD=1", 7000, response, sizeof(response));
    if (strstr(response, "POWERED DOWN") != NULL || strstr(response, "OK") != NULL)
    {
        return;
    }

    // Alternate shutdown command used by some modem firmwares.
    send_at_command("AT+CPOF", 5000, response, sizeof(response));
    if (strstr(response, "OK") != NULL)
    {
        return;
    }

    // Last resort: hardware shutdown using PWRKEY pulse.
    printf("AT shutdown commands failed; forcing PWRKEY shutdown.\n");
    gpio_put(MODEM_PWRKEY_PIN, 0);
    sleep_ms(2000);
    gpio_put(MODEM_PWRKEY_PIN, 1);
    sleep_ms(3000);
}

static bool connect_to_network(void)
{
    char response[1024];
    char command[64];

    send_at_command("AT", 1000, response, sizeof(response));
    if (strstr(response, "OK") == NULL)
    {
        printf("Modem not responding. Waking modem...\n");
        toggle_modem_power();
        sleep_ms(10000);

        send_at_command("AT", 2000, response, sizeof(response));
        if (strstr(response, "OK") == NULL)
        {
            printf("Modem still unavailable.\n");
            return false;
        }
    }

    snprintf(command, sizeof(command), "AT+CPIN=\"%s\"", SIM_PIN);
    send_at_command(command, 5000, response, sizeof(response));
    sleep_ms(5000);
    return true;
}

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

static int calculate_battery_percentage(float voltage)
{
    int centivolts = (int)(voltage * 100.0f);
    int percentage = map_int(centivolts, 1110, 1260, 0, 100);
    return clamp_int(percentage, 0, 100);
}

static void perform_upload(void)
{
    char response[2048];
    char command[320];
    char json[128];

    // 1) Read sensor with switched power.
    gpio_put(SENSOR_POWER_PIN, 1);
    sleep_ms(500);
    int moisture_raw = (int)read_adc_gpio(MOISTURE_PIN);
    gpio_put(SENSOR_POWER_PIN, 0);

    // 2) Wake/connect modem.
    if (!connect_to_network())
    {
        printf("Upload skipped: no modem/network.\n");
        return;
    }

    // 3) Prepare payload.
    float battery_volts = read_battery_voltage();
    int battery_pct = calculate_battery_percentage(battery_volts);
    int moisture_pct = map_int(moisture_raw, AIR_VALUE, WATER_VALUE, 0, 100);
    moisture_pct = clamp_int(moisture_pct, 0, 100);

    snprintf(json, sizeof(json), "{\"trailId\":\"%s\",\"moisture\":%d,\"battery\":%d}", TRAIL_ID, moisture_pct, battery_pct);
    printf("Sending JSON: %s\n", json);

    // 4) Send HTTP POST through modem AT commands.
    send_at_command("AT+HTTPINIT", 2000, response, sizeof(response));

    snprintf(command, sizeof(command), "AT+HTTPPARA=\"URL\",\"%s\"", API_URL);
    send_at_command(command, 2500, response, sizeof(response));

    send_at_command("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 1000, response, sizeof(response));

    snprintf(command, sizeof(command), "AT+HTTPPARA=\"USERDATA\",\"x-api-key: %s\"", API_KEY);
    send_at_command(command, 1500, response, sizeof(response));

    snprintf(command, sizeof(command), "AT+HTTPDATA=%u,5000", (unsigned int)strlen(json));
    send_at_command(command, 2000, response, sizeof(response));

    uart_puts(MODEM_UART, json);
    sleep_ms(2000);

    send_at_command("AT+HTTPACTION=1", 12000, response, sizeof(response));
    if (strstr(response, "+HTTPACTION: 1,200") == NULL)
    {
        printf("HTTP POST may have failed. Check modem response above.\n");
    }

    send_at_command("AT+HTTPTERM", 1000, response, sizeof(response));

    // 5) Power down modem.
    shutdown_modem();
}

int main(void)
{
    stdio_init_all();
    setvbuf(stdout, NULL, _IONBF, 0);
    sleep_ms(3000);
    printf("Booting Live Trail uploader (%s %s)...\n", __DATE__, __TIME__);

    uart_init(MODEM_UART, MODEM_BAUDRATE);
    gpio_set_function(MODEM_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(MODEM_RX_PIN, GPIO_FUNC_UART);

    gpio_init(SENSOR_POWER_PIN);
    gpio_set_dir(SENSOR_POWER_PIN, GPIO_OUT);
    gpio_put(SENSOR_POWER_PIN, 0);

    gpio_init(MODEM_PWRKEY_PIN);
    gpio_set_dir(MODEM_PWRKEY_PIN, GPIO_OUT);
    gpio_put(MODEM_PWRKEY_PIN, 1);

    adc_init();
    adc_gpio_init(MOISTURE_PIN);
    adc_gpio_init(BATTERY_PIN);

    printf("Waiting 10 seconds for modem stabilization...\n");
    sleep_with_progress(10000, 2000, "Startup wait");
    printf("Power saver mode initialized.\n");

    while (true)
    {
        printf("Starting upload cycle...\n");
        perform_upload();

        printf("Entering sleep for 6 hours...\n");
        sleep_with_progress(SLEEP_DURATION_MS, 60000, "Deep sleep");
    }

    return 0;
}
