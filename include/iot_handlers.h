/**
 * @file iot_handlers.h
 * @brief IoT Data Handlers for Water Tank Device
 *
 * Functions for handling sensor reading, pump control, display updates,
 * button inputs, and system commands using ArduinoIoTDevice library.
 */

#ifndef IOT_HANDLERS_H
#define IOT_HANDLERS_H

#include <Arduino.h>

/**
 * @brief Read sensors and update telemetry data
 * Updates iotDevice.telemetryData with current sensor readings
 */
void readSensors();

/**
 * @brief Calculate inflow rate based on water height change
 * @param currentHeight Current water height in cm
 * @return Flow rate in liters per minute
 */
float calculateInflowRate(float currentHeight);

/**
 * @brief Control pump based on auto/manual mode
 * In auto mode: Uses threshold-based control with hysteresis
 * In manual mode: Uses server/app command
 */
void controlPump();

/**
 * @brief Update display with current data
 * Cycles through status, network, and settings screens
 */
void updateDisplay();

/**
 * @brief Handle button inputs
 * BTN1: Cycle display screens
 * BTN2: Manual pump ON
 * BTN3: Toggle Auto/Manual mode
 * BTN4: Manual pump OFF
 * BTN5: WiFi reset (long press)
 */
void handleButtons();

/**
 * @brief Handle system commands from server
 * - config_update: Fetch new configuration
 * - force_update: Trigger OTA firmware update
 * - auto_update: Enable/disable automatic config updates
 */
void handleSystemCommands();

#endif // IOT_HANDLERS_H
