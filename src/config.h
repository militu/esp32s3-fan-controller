#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/*******************************************************************************
 * System Configuration
 ******************************************************************************/

// System States
enum class SystemState {
    STARTING,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_ERROR,
    RUNNING_WITH_WIFI,
    RUNNING_WITHOUT_WIFI
};

// System Status LED
#define STATUS_LED_PIN 33

// Debug Configuration
#define DEBUG_WIFI true
#define DEBUG_TEMP false
#define DEBUG_FAN  true
#define DEBUG_MAIN false
#define DEBUG_MQTT true
#define DEBUG_DISPLAY true
#define DEBUG_TM false
#define DEBUG_NTP true
#define DEBUG_INITIALIZER true

/*******************************************************************************
 * WiFi Configuration
 ******************************************************************************/
#define WIFI_SSID           "Wokwi-GUEST"
#define WIFI_PASSWORD       ""
#define WIFI_MAX_RETRIES    3
#define WIFI_CHECK_INTERVAL 300000  // 5 minutes
#define WIFI_RETRY_DELAY    3000    // 3 seconds
#define WIFI_BACKOFF_FACTOR   2        // For exponential backoff


/*******************************************************************************
 * NTP Configuration
 ******************************************************************************/
#define NTP_SYNC_INTERVAL   3600000  // 1 hour
#define NTP_SYNC_TIMEOUT    10000    // 10 seconds
#define NTP_SERVER          "pool.ntp.org"
#define NTP_BACKUP_SERVER   "time.nist.gov"
#define NTP_RETRY_DELAY    3000
#define NTP_BACKOFF_FACTOR   2

/*******************************************************************************
 * Temperature Sensor Configuration
 ******************************************************************************/
#define TEMP_SENSOR_PIN     14
#define TEMP_READ_INTERVAL  5000         // 1 second
#define TEMP_MAX_RETRIES    3
#define TEMP_SMOOTH_SAMPLES 1
#define TEMP_DEFAULT_VALUE  25.0
#define TEMP_READ_TIMEOUT   1000         // 1 second

// Temperature Control Thresholds
#define DEFAULT_TARGET_TEMP 27.0         // Target temperature
#define DEFAULT_MIN_TEMP    25.0         // Min temp for fan operation
#define DEFAULT_MAX_TEMP    45.0         // Max temp for full fan speed

/*******************************************************************************
 * Fan Hardware Configuration
 ******************************************************************************/
// PWM Settings
#define FAN_PWM_PIN         17           // PWM control pin
#define FAN_TACH_PIN        16           // Tachometer input pin
#define PWM_FREQUENCY       25000        // 25kHz for 4-wire PWM fans
#define PWM_RESOLUTION      8            // 8-bit resolution (0-255)
#define PWM_CHANNEL         0

// Speed Limits
#define FAN_MIN_SPEED         10           // ~10% PWM
#define FAN_MAX_SPEED         100          // 100% PWM
#define FAN_MIN_PWM         26           // ~10% PWM - used for conversion
#define FAN_MAX_PWM         255          // 100% PWM - used for conversion

#define RPM_MINIMUM         300          // Minimum expected RPM
#define RPM_MAXIMUM         3300         // Maximum expected RPM

// Control Parameters
#define FAN_PULSES_PER_REV  2            // Pulses per revolution
#define RPM_UPDATE_INTERVAL 3000          // Update RPM every second
#define FAN_MIN_RUNTIME     10000         // Min runtime before speed change
#define FAN_RAMP_STEP       5             // PWM change per step
#define FAN_RAMP_INTERVAL   350           // Ms between PWM changes

// Fan Status Codes
#define FAN_STATUS_OK           0
#define FAN_STATUS_LOW_RPM      1
#define FAN_STATUS_STALLED      2
#define FAN_STATUS_OVERCURRENT  3
#define FAN_STATUS_ERROR        4

// Fan Control Modes
enum class FanMode {
    AUTO,
    MANUAL,
    ERROR
};

/*******************************************************************************
 * Night Mode Configuration
 ******************************************************************************/
#define NIGHT_MODE_START    22            // 24-hour format
#define NIGHT_MODE_END      7
#define NIGHT_MODE_MAX_SPEED  40          // 40% maximum speed at night

/*******************************************************************************
 * MQTT Configuration
 ******************************************************************************/
// Connection Settings
#define MQTT_SERVER            "broker.hivemq.com"
#define MQTT_PORT              1883
#define MQTT_CLIENT_ID         "esp32_fan_controller"
#define MQTT_RECONNECT_DELAY   5000
#define MQTT_UPDATE_INTERVAL   5000   // 5 seconds
#define MQTT_MAX_RETRIES      3

// Base Topic
#define MQTT_BASE_TOPIC "fan_controller_esp_32"

// Status & Control Topics
#define MQTT_FAN_STATE_TOPIC             MQTT_BASE_TOPIC "/status"
#define MQTT_FAN_COMMAND_TOPIC           MQTT_BASE_TOPIC "/mode"
#define MQTT_FAN_PRESET_STATE_TOPIC      MQTT_BASE_TOPIC "/status"
#define MQTT_FAN_PRESET_COMMAND_TOPIC    MQTT_BASE_TOPIC "/mode"
#define MQTT_FAN_PERCENTAGE_STATE_TOPIC  MQTT_BASE_TOPIC "/status"
#define MQTT_FAN_PERCENTAGE_COMMAND_TOPIC MQTT_BASE_TOPIC "/mode"

// Sensor Topics
#define MQTT_TEMPERATURE_STATE_TOPIC     MQTT_BASE_TOPIC "/status"
#define MQTT_RPM_STATE_TOPIC            MQTT_BASE_TOPIC "/status"

// Night Mode Topics
#define MQTT_NIGHT_MODE_STATE_TOPIC      MQTT_BASE_TOPIC "/status"
#define MQTT_NIGHT_MODE_COMMAND_TOPIC    MQTT_BASE_TOPIC "/night_mode"
#define MQTT_NIGHT_SETTINGS_STATE_TOPIC  MQTT_BASE_TOPIC "/status"
#define MQTT_NIGHT_SETTINGS_COMMAND_TOPIC MQTT_BASE_TOPIC "/night_settings"

// System Topics
#define MQTT_RECOVERY_TOPIC              MQTT_BASE_TOPIC "/recovery"
#define MQTT_AVAILABILITY_TOPIC          MQTT_BASE_TOPIC "/available"

#endif // CONFIG_H