#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- HARDWARE PINOUT (RIDY-10-LITE + ESP32-S3) ---
#define PIN_TOUCH           1       // Internal copper foil capacitive electrode
#define PIN_LED_WHITE       4       // Cool White PWM -> R3 (47k) -> Q1 (AO3400A)
#define PIN_LED_YELLOW      5       // Warm Yellow PWM -> R2 (47k) -> Q2 (AO3400A)

// --- STAGE 2 RESERVED PERIPHERAL PINS ---
#define PIN_I2C_SDA         8       // BH1750 Ambient Light Sensor SDA (3.3V)
#define PIN_I2C_SCL         9       // BH1750 Ambient Light Sensor SCL (3.3V)
#define PIN_RADAR_TX        17      // ESP32 TX -> HLK-LD2410 RX (UART)
#define PIN_RADAR_RX        18      // ESP32 RX <- HLK-LD2410 TX (UART)
#define RADAR_BAUD_RATE     256000  // HLK-LD2410 default serial speed

// --- LED PWM DRIVER SETTINGS ---
#define PWM_FREQ            1000    // 1 kHz (optimal for 47k gate resistors on AO3400A)
#define PWM_RES             10      // 10-bit resolution (0 - 1023)
#define PWM_MAX             1023
#define CH_WHITE            0       // LEDC Channel 0
#define CH_YELLOW           1       // LEDC Channel 1

// --- LIGHTING & SOFT-FADE DYNAMICS ---
#define BRIGHTNESS_MIN      0.10f   // 10% minimum dimming floor
#define BRIGHTNESS_MAX      1.00f   // 100% ceiling
#define BRIGHTNESS_DEFAULT  0.80f   // Default startup brightness
#define MIX_CHANNEL_LIMIT   0.55f   // Power cap per channel in 50/50 mode (protects 2A PSU)
#define DIM_SPEED_PER_SEC   0.45f   // Full dimming sweep speed (~2.2 sec)
#define FADE_SPEED_SWITCH   8.0f    // Smooth ON/OFF/mode transition speed
#define FADE_SPEED_DIMMING  25.0f   // Fast tracking during manual touch dimming

// --- CAPACITIVE TOUCH CALIBRATION & TIMINGS ---
#define TOUCH_MEAS_CYCLES   0x0800  // Зменшений цикл вимірювання (тільки контактний дотик)
#define TOUCH_SLEEP_CYCLES  0x1000
#define TOUCH_CALIB_SAMPLES 30
#define TOUCH_FILTER_SAMPLES 4
#define TOUCH_DELTA_ON      3500    // Поріг спрацьовування для циклу 0x0800
#define TOUCH_DELTA_OFF     2000    // Поріг відпускання
#define TOUCH_DEBOUNCE_MS   45
#define TOUCH_LONG_PRESS_MS 450

// --- NETWORK, WEB UI & OTA DEFAULTS ---
#define WIFI_MAX_SLOTS      6       // Multi-AP roaming slots in NVS
#define WIFI_CONNECT_TIMEOUT_MS 10000
#define FALLBACK_AP_SSID    "RidyLamp-AP"
#define FALLBACK_AP_PASS    "12345678"
#define DEFAULT_HOSTNAME    "ridy-lamp"
#define DEFAULT_ADMIN_USER  "admin"
#define DEFAULT_ADMIN_PASS  "admin"
// --- NVS STATE PERSISTENCE (WEAR LEVELING PROTECTION) ---
#define NVS_SAVE_DELAY_MS   3000    // Write state to Flash 3s after last user interaction
// --- SYSTEM RING-BUFFER LOGGER ---
#define SYSLOG_MAX_ENTRIES  12      // Fixed ring buffer size (zero heap fragmentation)
#define SYSLOG_LINE_LEN     84      // Max characters per log entry
// --- ROUND TFT DISPLAY (1.28" 240x240 GC9A01, 7-PIN SPI) ---
// #define TFT_WIDTH           240
// #define TFT_HEIGHT          240
// #define TFT_SCLK_PIN        12      // Blue/Black (SCL)
// #define TFT_MOSI_PIN        11      // Blue (SDA)
// #define TFT_CS_PIN          10      // Yellow (CS)
// #define TFT_DC_PIN          13      // Green (DC)
// #define TFT_RST_PIN         14      // White (RST)
#define LINK_API_KEY        "ridy-desk-2026"
#endif // CONFIG_H