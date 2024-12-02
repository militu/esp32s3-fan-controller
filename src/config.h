#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <cstdint>

#define MQTT_BASE_TOPIC "fan_controller_amadeusmilitu_128932"
#define MQTT_TOPIC(suffix) MQTT_BASE_TOPIC "/" suffix

namespace Config {
    /**
     * @brief System-wide configuration settings
     */
    namespace System {
        enum class State {
            STARTING,
            WIFI_CONNECTING,
            WIFI_CONNECTED,
            WIFI_ERROR,
            RUNNING_WITH_WIFI,
            RUNNING_WITHOUT_WIFI
        };

        constexpr uint8_t STATUS_LED_PIN = 33;

        namespace Debug {
            constexpr bool WIFI = false;
            constexpr bool TEMP = false;
            constexpr bool FAN = false;
            constexpr bool MAIN = false;
            constexpr bool MQTT = false;
            constexpr bool LVGL = true;
            constexpr bool TM = false;
            constexpr bool NTP = false;
            constexpr bool INITIALIZER = false;
            constexpr bool PERSISTENT = false;
        }
    }

    /**
     * @brief WiFi connection configuration
     */
    namespace WiFi {
        constexpr char SSID[] = "Wokwi-GUEST";
        constexpr char PASSWORD[] = "";
        constexpr uint8_t MAX_RETRIES = 3;
        constexpr uint32_t CHECK_INTERVAL = 60000;    // 1 minute
        constexpr uint32_t RETRY_DELAY = 3000;        // 3 seconds
        constexpr uint8_t BACKOFF_FACTOR = 2;         // For exponential backoff
    }

    /**
     * @brief NTP time synchronization configuration
     */
    namespace NTP {
        constexpr uint32_t SYNC_INTERVAL = 3600000;   // 1 hour
        constexpr uint32_t SYNC_TIMEOUT = 5000;       // 5 seconds
        constexpr char SERVER[] = "pool.ntp.org";
        constexpr char BACKUP_SERVER[] = "time.nist.gov";
        constexpr uint32_t RETRY_DELAY = 3000;        // 3 seconds
        constexpr uint8_t BACKOFF_FACTOR = 2;
        constexpr uint8_t MAX_SYNC_ATTEMPTS = 3;
    }

    /**
     * @brief Temperature sensor configuration
     */
    namespace Temperature {
        constexpr uint8_t SENSOR_PIN = 14;
        constexpr uint32_t READ_INTERVAL = 2000;      // 2 seconds
        constexpr uint8_t MAX_RETRIES = 3;
        constexpr uint8_t SMOOTH_SAMPLES = 5;         // Rolling average samples
        constexpr float DEFAULT_VALUE = 25.0f;
        constexpr uint32_t READ_TIMEOUT = 1000;       // 1 second

        namespace Control {
            constexpr float DEFAULT_TARGET = 27.0f;    // Target temperature
            constexpr float MIN_TEMP = 25.0f;         // Min temp for fan
            constexpr float MAX_TEMP = 45.0f;         // Max temp for fan
        }
    }

    /**
     * @brief Fan hardware and control configuration
     */
    namespace Fan {
        namespace PWM {
            constexpr uint8_t PIN = 17;
            constexpr uint8_t TACH_PIN = 16;
            constexpr uint32_t FREQUENCY = 25000;     // 25kHz
            constexpr uint8_t RESOLUTION = 8;         // 8-bit
            constexpr uint8_t CHANNEL = 0;
        }

        namespace Speed {
            constexpr uint8_t MIN_PERCENT = 10;
            constexpr uint8_t MAX_PERCENT = 100;
            constexpr uint8_t MIN_PWM = 26;           // ~10% duty
            constexpr uint8_t MAX_PWM = 255;          // 100% duty
        }

        namespace RPM {
            constexpr uint16_t MINIMUM = 300;
            constexpr uint16_t MAXIMUM = 3300;
            constexpr uint8_t PULSES_PER_REV = 2;
            constexpr uint32_t UPDATE_INTERVAL = 1000;  // 1 second
        }

        namespace Control {
            constexpr uint32_t MIN_RUNTIME = 10000;    // Min time before speed change
            constexpr uint8_t RAMP_STEP = 5;           // PWM change per step
            constexpr uint32_t RAMP_INTERVAL = 250;    // Ms between changes
        }

        enum class Mode {
            AUTO,
            MANUAL,
            ERROR
        };
    }

    /**
     * @brief Night mode operation configuration
     */
    namespace NightMode {
        constexpr uint8_t START_HOUR = 22;
        constexpr uint8_t END_HOUR = 7;
        constexpr uint8_t MAX_SPEED = 40;             // 40% maximum at night
    }

    /**
     * @brief MQTT communication configuration
     */
    namespace MQTT {
        constexpr char SERVER[] = "broker.hivemq.com";
        constexpr uint16_t PORT = 1883;
        constexpr char CLIENT_ID[] = "esp32_fan_controller";
        constexpr uint32_t RECONNECT_DELAY = 5000;
        constexpr uint32_t UPDATE_INTERVAL = 10000;
        constexpr uint8_t MAX_RETRIES = 3;

        namespace Topics {
            constexpr char BASE[] = MQTT_BASE_TOPIC;
            
            // Status topics
            constexpr char SYSTEM_STATUS[] = MQTT_TOPIC("status/system");
            constexpr char NIGHT_MODE_STATUS[] = MQTT_TOPIC("status/night_mode");
            constexpr char AVAILABILITY[] = MQTT_TOPIC("availability");

            // Control topics
            constexpr char MODE_SET[] = MQTT_TOPIC("control/mode/set");
            constexpr char NIGHT_MODE_SET[] = MQTT_TOPIC("control/night_mode/set");
            constexpr char NIGHT_SETTINGS_SET[] = MQTT_TOPIC("control/night_settings/set");
            constexpr char RECOVERY_SET[] = MQTT_TOPIC("control/recovery/set");
        }
    }
}

#endif // CONFIG_H