/**
 * @file IoTField.h
 * @brief Template class for IoT data fields with automatic timestamp tracking
 *
 * Provides a field structure that holds a value and lastModified timestamp.
 * Supports automatic JSON serialization/deserialization.
 */

#ifndef IOT_FIELD_H
#define IOT_FIELD_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief Generic field class with value and timestamp
 * @tparam T The data type of the field (int, float, String, bool, etc.)
 */
template<typename T>
class IoTField {
public:
    T value;                    // The field value
    uint64_t lastModified;      // Unix timestamp in milliseconds
    String key;                 // Field key name for JSON
    String label;               // Human-readable label (optional)
    String type;                // Field type for server (optional)

    /**
     * @brief Default constructor
     */
    IoTField() : value(T()), lastModified(0), key(""), label(""), type("") {}

    /**
     * @brief Constructor with key
     */
    IoTField(const String& fieldKey)
        : value(T()), lastModified(0), key(fieldKey), label(""), type("") {}

    /**
     * @brief Constructor with key and initial value
     */
    IoTField(const String& fieldKey, T initialValue)
        : value(initialValue), lastModified(0), key(fieldKey), label(""), type("") {}

    /**
     * @brief Constructor with all parameters
     */
    IoTField(const String& fieldKey, T initialValue, const String& fieldLabel, const String& fieldType)
        : value(initialValue), lastModified(0), key(fieldKey), label(fieldLabel), type(fieldType) {}

    /**
     * @brief Assignment operator for easy value setting
     * Automatically updates lastModified if timestamp provider is set
     */
    IoTField& operator=(const T& newValue) {
        if (value != newValue) {
            value = newValue;
            // Note: lastModified should be set by the library when this field changes
        }
        return *this;
    }

    /**
     * @brief Implicit conversion to T for easy value reading
     */
    operator T() const {
        return value;
    }

    /**
     * @brief Check if this field is newer than another
     */
    bool isNewerThan(const IoTField<T>& other) const {
        return this->lastModified > other.lastModified;
    }

    /**
     * @brief Check if this field is older than another
     */
    bool isOlderThan(const IoTField<T>& other) const {
        return this->lastModified < other.lastModified;
    }

    /**
     * @brief Serialize to JSON object (server format with nested structure)
     */
    void toJson(JsonObject& obj) const {
        obj["key"] = key;
        if (label.length() > 0) obj["label"] = label;
        if (type.length() > 0) obj["type"] = type;

        // Handle different types
        if constexpr (std::is_same_v<T, String>) {
            obj["value"] = value.c_str();
        } else {
            obj["value"] = value;
        }

        if (lastModified > 0) {
            obj["lastModified"] = lastModified;
        }
    }

    /**
     * @brief Deserialize from JSON object (server format)
     */
    bool fromJson(const JsonObject& obj) {
        if (!obj.containsKey("value")) return false;

        if (obj.containsKey("key")) key = obj["key"].as<String>();
        if (obj.containsKey("label")) label = obj["label"].as<String>();
        if (obj.containsKey("type")) type = obj["type"].as<String>();

        // Handle different types
        if constexpr (std::is_same_v<T, String>) {
            value = obj["value"].as<String>();
        } else if constexpr (std::is_same_v<T, bool>) {
            value = obj["value"].as<bool>();
        } else if constexpr (std::is_same_v<T, int>) {
            value = obj["value"].as<int>();
        } else if constexpr (std::is_same_v<T, float>) {
            value = obj["value"].as<float>();
        } else if constexpr (std::is_same_v<T, double>) {
            value = obj["value"].as<double>();
        } else {
            value = obj["value"].as<T>();
        }

        if (obj.containsKey("lastModified")) {
            lastModified = obj["lastModified"].as<uint64_t>();
        }

        return true;
    }
};

/**
 * @brief Macro to easily define IoT fields in structs
 * Usage: IOT_FIELD(float, temperature, "temperature", "Temperature", "number")
 */
#define IOT_FIELD(type, name, keyName, labelName, typeName) \
    IoTField<type> name{keyName, type(), labelName, typeName}

/**
 * @brief Macro to define simple IoT fields without label/type
 * Usage: IOT_FIELD_SIMPLE(float, temperature, "temperature")
 */
#define IOT_FIELD_SIMPLE(type, name, keyName) \
    IoTField<type> name{keyName}

#endif // IOT_FIELD_H
