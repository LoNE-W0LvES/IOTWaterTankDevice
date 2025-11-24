#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "iot_config.h"  // Include IoT library configuration

// ============================================================================
// DEVICE-SPECIFIC CONFIGURATION
// Hardware pins, timing, and device behavior settings
// ============================================================================

// ============================================================================
// DEBUG CONFIGURATION
// ============================================================================

// Comment out to disable debug output
#define DEBUG_ENABLED

// Comment out to disable verbose HTTP response logging for API client
// When disabled, still shows request info and status, but hides response bodies
#define DEBUG_RESPONSE_API

// Comment out to disable verbose HTTP response logging for webserver
// When disabled, still shows request info and status, but hides response bodies
// #define DEBUG_RESPONSE_WEBSERVER

#ifdef DEBUG_ENABLED
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(format, ...) Serial.printf(format, ##__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(format, ...)
#endif

#ifdef DEBUG_RESPONSE_API
  #define DEBUG_RESPONSE_API_PRINT(x) Serial.print(x)
  #define DEBUG_RESPONSE_API_PRINTLN(x) Serial.println(x)
  #define DEBUG_RESPONSE_API_PRINTF(format, ...) Serial.printf(format, ##__VA_ARGS__)
#else
  #define DEBUG_RESPONSE_API_PRINT(x)
  #define DEBUG_RESPONSE_API_PRINTLN(x)
  #define DEBUG_RESPONSE_API_PRINTF(format, ...)
#endif

#ifdef DEBUG_RESPONSE_WEBSERVER
  #define DEBUG_RESPONSE_WS_PRINT(x) Serial.print(x)
  #define DEBUG_RESPONSE_WS_PRINTLN(x) Serial.println(x)
  #define DEBUG_RESPONSE_WS_PRINTF(format, ...) Serial.printf(format, ##__VA_ARGS__)
#else
  #define DEBUG_RESPONSE_WS_PRINT(x)
  #define DEBUG_RESPONSE_WS_PRINTLN(x)
  #define DEBUG_RESPONSE_WS_PRINTF(format, ...)
#endif

// ============================================================================
// HARDWARE PIN CONFIGURATION
// ============================================================================

// OLED Display (SSD1306 I2C)
#define OLED_SDA_PIN 8
#define OLED_SCL_PIN 9
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_RESET -1

// Ultrasonic Sensor (HC-SR04)
#define ULTRASONIC_TRIG_PIN 6
#define ULTRASONIC_ECHO_PIN 7

// Relay Control (Pump)
#define RELAY_PIN 5

// Button Inputs
#define BTN1_PIN 10  // Cycle display screens
#define BTN2_PIN 46  // Manual pump ON
#define BTN3_PIN 3   // Toggle Auto/Manual mode
#define BTN4_PIN 18  // Manual pump OFF
#define BTN5_PIN 17  // Reset WiFi (long press)
#define BTN6_PIN 12  // Hardware override switch

// ============================================================================
// DEVICE TIMING CONFIGURATION
// ============================================================================

#define SENSOR_READ_INTERVAL 1000       // 1 second - sensor reading
#define DISPLAY_UPDATE_INTERVAL 500     // 0.5 seconds - update display

// ============================================================================
// BUTTON CONFIGURATION
// ============================================================================

#define BUTTON_DEBOUNCE_MS 50
#define BUTTON_LONG_PRESS_MS 10000  // 10 seconds for WiFi reset

// ============================================================================
// DEFAULT VALUES
// ============================================================================

#define DEFAULT_TANK_HEIGHT 100.0f
#define DEFAULT_UPPER_THRESHOLD 85.0f
#define DEFAULT_LOWER_THRESHOLD 20.0f
#define DEFAULT_TANK_WIDTH 50.0f

// ============================================================================
// SENSOR CONFIGURATION
// ============================================================================

// Spike detection threshold (cm)
// Maximum realistic water level change per measurement interval (500ms)
// 20cm/0.5s = 40cm/s is extremely fast for a water tank
// Adjust lower for slower systems, higher for fast-filling industrial tanks
#define SENSOR_SPIKE_THRESHOLD 20.0f  // cm

// Sensor filter enable/disable
// When enabled, applies filtering/smoothing to sensor readings
// Can be toggled via device config from server or app
#define DEFAULT_SENSOR_FILTER true

#endif // CONFIG_H
