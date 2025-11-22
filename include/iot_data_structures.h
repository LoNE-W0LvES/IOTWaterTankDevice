/**
 * @file iot_data_structures.h
 * @brief Water Tank IoT Data Structures
 *
 * Defines custom data structures using IoTField template for automatic
 * timestamp tracking and 3-way merge support.
 */

#ifndef IOT_DATA_STRUCTURES_H
#define IOT_DATA_STRUCTURES_H

#include <Arduino.h>
#include <IoTField.h>
#include "config.h"

// ============================================================================
// DEVICE CONFIGURATION STRUCTURE
// ============================================================================

/**
 * @brief Custom device configuration fields for water tank
 * These fields are synced with the server and stored persistently
 */
struct WaterTankDeviceConfig {
    IoTField<float> tankHeight{"tankHeight", DEFAULT_TANK_HEIGHT, "Tank Height (cm)", "number"};
    IoTField<float> tankWidth{"tankWidth", DEFAULT_TANK_WIDTH, "Tank Width (cm)", "number"};
    IoTField<String> tankShape{"tankShape", "Cylindrical", "Tank Shape", "string"};
    IoTField<float> upperThreshold{"upperThreshold", DEFAULT_UPPER_THRESHOLD, "Upper Threshold (%)", "number"};
    IoTField<float> lowerThreshold{"lowerThreshold", DEFAULT_LOWER_THRESHOLD, "Lower Threshold (%)", "number"};
    IoTField<float> maxInflow{"maxInflow", 100.0f, "Max Inflow (L/min)", "number"};
    IoTField<float> usedTotal{"usedTotal", 0.0f, "Total Water Used (L)", "number"};
    IoTField<bool> sensorFilter{"sensorFilter", DEFAULT_SENSOR_FILTER, "Sensor Filter", "boolean"};
};

// ============================================================================
// CONTROL DATA STRUCTURE
// ============================================================================

/**
 * @brief Custom control data fields for water tank
 * These fields are updated by the server/app to control the device
 */
struct WaterTankControlData {
    IoTField<bool> pumpSwitch{"pumpSwitch", false, "Pump Switch", "boolean"};
    IoTField<bool> autoMode{"autoMode", true, "Auto Mode", "boolean"};
};

// ============================================================================
// TELEMETRY DATA STRUCTURE
// ============================================================================

/**
 * @brief Custom telemetry fields for water tank
 * These fields are read by sensors and uploaded to the server
 */
struct WaterTankTelemetryData {
    IoTField<float> waterLevel{"waterLevel", 0.0f, "Water Level (%)", "number"};
    IoTField<float> currInflow{"currInflow", 0.0f, "Current Inflow (L/min)", "number"};
    IoTField<int> pumpStatus{"pumpStatus", 0, "Pump Status", "number"};
    IoTField<float> waterHeight{"waterHeight", 0.0f, "Water Height (cm)", "number"};
};

#endif // IOT_DATA_STRUCTURES_H
