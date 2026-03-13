#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>

#include "app_secrets.h"

// --- User Configuration ---
#define APP_SIM_PIN "1408"
#define APP_TRAIL_ID "2"
#define APP_API_URL "https://live-trail-server.vercel.app/api/data"

// --- Pin Definitions (Raspberry Pi Pico / RP2040) ---
#define MODEM_TX_PIN 1
#define MODEM_RX_PIN 0
#define MOISTURE_PIN 26
#define BATTERY_PIN 27
#define SENSOR_POWER_PIN 22
#define MODEM_PWRKEY_PIN 2

// --- Calibration Constants ---
#define AIR_VALUE 500
#define WATER_VALUE 200

// Sleep interval between uploads (6 hours).
#define SLEEP_SECONDS ((uint32_t)21600)

#define MODEM_BAUDRATE 115200

#endif
