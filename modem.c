#include "modem.h"

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/uart.h"

#include "app_config.h"

#define MODEM_UART uart0

static bool modem_pwrkey_claimed = false;
static bool modem_uart_ready = false;

static void modem_uart_enable(void)
{
    if (modem_uart_ready)
    {
        return;
    }

    uart_init(MODEM_UART, MODEM_BAUDRATE);
    gpio_set_function(MODEM_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(MODEM_RX_PIN, GPIO_FUNC_UART);
    modem_uart_ready = true;
}

static void modem_uart_disable(void)
{
    if (!modem_uart_ready)
    {
        return;
    }

    uart_deinit(MODEM_UART);

    // High-Z both UART lines to avoid back-powering the modem through IO.
    gpio_set_function(MODEM_TX_PIN, GPIO_FUNC_SIO);
    gpio_set_function(MODEM_RX_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(MODEM_TX_PIN, GPIO_IN);
    gpio_set_dir(MODEM_RX_PIN, GPIO_IN);
    gpio_disable_pulls(MODEM_TX_PIN);
    gpio_disable_pulls(MODEM_RX_PIN);

    modem_uart_ready = false;
}

static void modem_claim_pwrkey_output(void)
{
    if (!modem_pwrkey_claimed)
    {
        gpio_set_dir(MODEM_PWRKEY_PIN, GPIO_OUT);
        modem_pwrkey_claimed = true;
    }

    // Idle level for SIMCom boards: keep PWRKEY high.
    gpio_put(MODEM_PWRKEY_PIN, 1);
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

        // HTTPACTION is async so OK only means the request was accepted.
        if (strstr(response, "POWER DOWN") ||
            strstr(response, "DOWNLOAD") ||
            strstr(response, "+CPIN: READY") ||
            (strstr(response, "\nOK") && !strstr(command, "HTTPACTION")))
        {
            break;
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
    modem_claim_pwrkey_output();

    printf("Toggling modem power key...\n");
    gpio_put(MODEM_PWRKEY_PIN, 0);
    sleep_ms(1500);
    gpio_put(MODEM_PWRKEY_PIN, 1);
    sleep_ms(3000);
}

static bool modem_wait_for_registration(uint32_t timeout_ms)
{
    char reg_buf[512] = {0};
    size_t reg_idx = 0;
    absolute_time_t reg_start = get_absolute_time();

    printf("Waiting for network registration...\n");

    while ((uint64_t)absolute_time_diff_us(reg_start, get_absolute_time()) < (uint64_t)timeout_ms * 1000ULL)
    {
        while (uart_is_readable(MODEM_UART) && reg_idx < sizeof(reg_buf) - 1)
        {
            reg_buf[reg_idx++] = (char)uart_getc(MODEM_UART);
            reg_buf[reg_idx] = '\0';
        }

        if (reg_idx > 0)
        {
            printf("REG: %s\n", reg_buf);
            if (strstr(reg_buf, "EPS PDN ACT") != NULL)
            {
                return true;
            }
        }

        sleep_ms(100);
    }

    return false;
}

void modem_init(void)
{
    gpio_init(MODEM_PWRKEY_PIN);
    modem_claim_pwrkey_output();
    modem_uart_enable();
}

bool modem_connect_network(void)
{
    char response[1024];
    char command[64];

    modem_uart_enable();
    modem_claim_pwrkey_output();

    send_at_command("AT+CSCLK=0", 500, response, sizeof(response));
    send_at_command("AT+CFUN=1", 3000, response, sizeof(response));
    sleep_ms(500);

    send_at_command("AT", 1000, response, sizeof(response));
    if (strstr(response, "OK") == NULL)
    {
        printf("Modem not responding. Toggling power...\n");
        toggle_modem_power();
        sleep_ms(10000);

        send_at_command("AT", 2000, response, sizeof(response));
        if (strstr(response, "OK") == NULL)
        {
            printf("Modem still unavailable.\n");
            return false;
        }
    }

    snprintf(command, sizeof(command), "AT+CPIN=\"%s\"", APP_SIM_PIN);
    send_at_command(command, 5000, response, sizeof(response));

    (void)modem_wait_for_registration(15000);
    return true;
}

bool modem_http_post_json(const char *url, const char *api_key, const char *json_payload)
{
    char response[2048];
    char command[320];
    bool http_ok = false;

    send_at_command("AT+HTTPINIT", 500, response, sizeof(response));

    snprintf(command, sizeof(command), "AT+HTTPPARA=\"URL\",\"%s\"", url);
    send_at_command(command, 1000, response, sizeof(response));

    send_at_command("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 1000, response, sizeof(response));

    snprintf(command, sizeof(command), "AT+HTTPPARA=\"USERDATA\",\"x-api-key: %s\"", api_key);
    send_at_command(command, 1000, response, sizeof(response));

    snprintf(command, sizeof(command), "AT+HTTPDATA=%u,1000", (unsigned int)strlen(json_payload));
    send_at_command(command, 2000, response, sizeof(response));

    uart_puts(MODEM_UART, json_payload);
    sleep_ms(50);

    send_at_command("AT+HTTPACTION=1", 2000, response, sizeof(response));

    printf("Waiting for HTTP response...\n");
    absolute_time_t http_start = get_absolute_time();

    while (absolute_time_diff_us(http_start, get_absolute_time()) < 12000000ULL)
    {
        char chunk[256] = {0};
        size_t idx = 0;

        while (uart_is_readable(MODEM_UART) && idx < sizeof(chunk) - 1)
        {
            chunk[idx++] = (char)uart_getc(MODEM_UART);
        }

        if (idx > 0)
        {
            printf("HTTP: %s\n", chunk);
            if (strstr(chunk, "+HTTPACTION: 1,200") != NULL)
            {
                http_ok = true;
                break;
            }
            if (strstr(chunk, "+HTTPACTION:") != NULL)
            {
                break;
            }
        }

        sleep_ms(100);
    }

    send_at_command("AT+HTTPTERM", 300, response, sizeof(response));
    return http_ok;
}

void modem_shutdown_for_sleep(void)
{
    char response[1024];

    printf("Sleeping modem...\n");

    send_at_command("AT+CFUN=0", 3000, response, sizeof(response));
    send_at_command("AT+CSCLK=2", 500, response, sizeof(response));

    modem_uart_disable();
    modem_claim_pwrkey_output();
}
