#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <cstdint>
#include "secrets.h"

#define MQTT_BASE_TOPIC "fan_controller"
#define MQTT_TOPIC(suffix) MQTT_BASE_TOPIC "/" suffix

namespace Config {
    /**
     * @brief Hardware-specific configuration settings
     */
    namespace Hardware {
        #ifdef USE_LILYGO_S3
            constexpr uint8_t PIN_BUTTON_1 = 0;
            constexpr uint8_t PIN_BUTTON_2 = 14;
        #else  // ILI9341 hardware
            constexpr uint8_t PIN_BUTTON_1 = 0;
            constexpr uint8_t PIN_BUTTON_2 = 7;
        #endif
    }


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
            constexpr bool SCREEN = false; // NOT DISPLAY because of conflict
            constexpr bool TASK_MANAGER = false;
            constexpr bool NTP = false;
            constexpr bool INITIALIZER = false;
            constexpr bool PERSISTENT = false;
        }
    }

    /**
     * @brief WiFi connection configuration
     */
    namespace WiFi {
        using Secrets::WiFi::SSID;
        using Secrets::WiFi::PASSWORD;
        constexpr uint8_t MAX_RETRIES = 3;
        constexpr uint32_t CONNECTION_CHECK_INTERVAL_MS = 60000;    // 1 minute
        constexpr uint32_t RETRY_DELAY_MS = 3000;        // 3 seconds
        constexpr uint8_t BACKOFF_FACTOR = 2;         // For exponential backoff

        namespace Task{
            constexpr uint32_t STACK_SIZE = 4096;
            constexpr UBaseType_t TASK_PRIORITY = 2;
            constexpr BaseType_t TASK_CORE = 0;
        }
    }

    /**
     * @brief NTP time synchronization configuration
     */
    namespace NTP {
        constexpr uint32_t SYNC_INTERVAL_MS = 3600000;   // 1 hour
        constexpr uint32_t SYNC_TIMEOUT_MS = 5000;       // 5 seconds
        constexpr char SERVER[] = "pool.ntp.org";
        constexpr char BACKUP_SERVER[] = "time.nist.gov";
        constexpr uint32_t RETRY_DELAY_MS = 3000;        // 3 seconds
        constexpr uint8_t BACKOFF_FACTOR = 2;
        constexpr uint8_t MAX_SYNC_ATTEMPTS = 3;

        namespace Task {
            constexpr uint32_t STACK_SIZE = 4096;
            constexpr UBaseType_t TASK_PRIORITY = 1;
            constexpr BaseType_t TASK_CORE = 1;

        }
    }

    /**
     * @brief Temperature sensor configuration
     */
    namespace Temperature {
        constexpr uint8_t SENSOR_PIN = 14;
        constexpr uint32_t READ_INTERVAL_MS = 2000;      // 2 seconds
        constexpr uint8_t MAX_RETRIES = 3;
        constexpr uint8_t SMOOTH_SAMPLES = 5;         // Rolling average samples
        constexpr float DEFAULT_VALUE = 25.0f;
        constexpr uint32_t READ_TIMEOUT_MS = 1000;       // 1 second
        
        namespace Task {
            constexpr uint32_t STACK_SIZE = 4096;
            constexpr UBaseType_t TASK_PRIORITY = 3;
            constexpr BaseType_t TASK_CORE = 1;
        }
    }

    /**
     * @brief Fan hardware and control configuration
     */
    namespace Fan {
        namespace PWM {
            constexpr uint8_t PWM_PIN = 17;
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
            constexpr uint32_t MIN_RUNTIME_MS = 10000;    // Min time before speed change
            constexpr uint8_t RAMP_STEP = 5;           // PWM change per step
            constexpr uint32_t RAMP_INTERVAL_MS = 250;    // Ms between changes
            constexpr float DEFAULT_TARGET = 27.0f;    // Target temperature
            constexpr float MIN_TRIGGER_TEMP = 25.0f;         // Min trigger temp for fan
            constexpr float MAX_TRIGGER_TEMP = 60.0f;         // Max trigger temp for fan
            constexpr uint32_t MUTEX_TIMEOUT_MS = 1000;
            constexpr uint8_t STALL_RETRY_COUNT = 3;
            constexpr uint32_t EVENT_CHECK_INTERVAL = 1000;
        }

        namespace NightMode {
            constexpr uint8_t START_HOUR = 22;
            constexpr uint8_t END_HOUR = 7;
            constexpr uint8_t MAX_SPEED_PERCENT = 40;             // 40% maximum at night
        }

        namespace Task {
            constexpr uint32_t STACK_SIZE = 4096;
            constexpr UBaseType_t TASK_PRIORITY = 3;
            constexpr BaseType_t TASK_CORE = 1;
        }

        enum class Mode {
            AUTO,
            MANUAL,
            ERROR
        };
    }

    /**
     * @brief MQTT communication configuration
     */
    namespace MQTT {
        using Secrets::MQTT::SERVER;
        using Secrets::MQTT::PORT;
        using Secrets::MQTT::CLIENT_ID;
        using Secrets::MQTT::USERNAME;
        using Secrets::MQTT::PASSWORD;
        constexpr uint32_t RECONNECT_DELAY_MS = 5000;
        constexpr uint32_t UPDATE_INTERVAL = 10000;
        constexpr uint8_t MAX_RETRIES = 3;
        constexpr size_t QUEUE_SIZE = 10;
        constexpr uint32_t QUEUE_TIMEOUT_MS = 100;
        constexpr uint32_t MUTEX_TIMEOUT_MS = 1000;
        constexpr uint32_t AVAILABILITY_INTERVAL = 30000;
        constexpr uint32_t CLIENT_LOOP_INTERVAL = 50;

        namespace Message {
            constexpr size_t MAX_TOPIC_LENGTH = 64;
            constexpr size_t MAX_PAYLOAD_LENGTH = 256;
        }

        namespace Task {
            constexpr uint32_t STACK_SIZE = 8192;
            constexpr UBaseType_t TASK_PRIORITY = 4;
            constexpr BaseType_t TASK_CORE = 1;
        }

        namespace Topics {
            constexpr char BASE[] = MQTT_BASE_TOPIC;
            constexpr char AVAILABILITY[] = MQTT_TOPIC("availability");

            namespace Status {
                constexpr char SYSTEM[] = MQTT_TOPIC("status/system");
                constexpr char NIGHT_MODE[] = MQTT_TOPIC("status/night_mode");
            }
            namespace Control {
                constexpr char MODE[] = MQTT_TOPIC("control/mode/set");
                constexpr char NIGHT_MODE[] = MQTT_TOPIC("control/night_mode/set");
                constexpr char NIGHT_SETTINGS[] = MQTT_TOPIC("control/night_settings/set");
                constexpr char RECOVERY[] = MQTT_TOPIC("control/recovery/set");
            }
        }
    }

    /**
     * @brief Task Manager configuration
     */
    namespace TaskManager {
        constexpr size_t MAX_TASKS = 10;
        constexpr size_t STACK_WARNING_THRESHOLD = 200;
    }

    /**
     * @brief Display configuration
     */
    namespace Display {

        namespace Sleep {
            constexpr uint32_t SCREEN_TIMEOUT_MS = 5 * 60 * 1000;  // 5 minutes
        }

        namespace DisplayRender {
            constexpr uint32_t STACK_SIZE = 4096;
            constexpr UBaseType_t TASK_PRIORITY = 4;  
            constexpr BaseType_t TASK_CORE = 0;
            constexpr uint32_t TASK_DELAY = 16;
            constexpr uint32_t UPDATE_INTERVAL = 100;
        }

        namespace DisplayUpdate {
            constexpr uint32_t STACK_SIZE = 4096;
            constexpr UBaseType_t TASK_PRIORITY = 2;
            constexpr BaseType_t TASK_CORE = 1;
        
            namespace Queue {
                constexpr uint8_t SIZE = 5;
            }
        }

        namespace Dashboard {
            constexpr float MARGIN_TO_WIDTH_RATIO = 0.18f;

            namespace TopBar {
                constexpr float HEIGHT_TO_SCREEN_RATIO = 0.18f;
                constexpr float SIDE_PADDING_RATIO = 0.01f;
                constexpr float ICON_GAP_RATIO = 0.1f;

            }

            namespace Meters {
                constexpr float METER_SIZE_RATIO = 0.43f;
                constexpr float WIDGET_TO_CONTAINER_RATIO = 0.98f;
                constexpr float CORNER_RADIUS_RATIO = 0.55f;
                constexpr float BOTTOM_OFFSET_RATIO = 0.08f;
                
                namespace Animation {
                    constexpr u_int16_t SPEED_MS = 2000;
                }

                namespace Temperature {
                    constexpr float GOOD_TO_WARNING_THRESHOLD = 25.0f;
                    constexpr float WARNING_TO_CRITICAL_THRESHOLD = 50.0f;
                    constexpr float SCALE_THICKNESS_RATIO = 0.12f;
                    constexpr uint8_t MIN_TEMP = 10;
                    constexpr uint8_t MAX_TEMP = 60;
                }

                namespace Fan {
                    constexpr uint8_t GOOD_TO_WARNING_THRESHOLD = 30;
                    constexpr uint8_t WARNING_TO_CRITICAL_THRESHOLD = 60;
                    constexpr float ARC_THICKNESS_RATIO = 0.05f;
                    constexpr float SCALE_THICKNESS_RATIO = 0.12f;
                    constexpr uint8_t MIN_SPEED = 0;
                    constexpr uint8_t MAX_SPEED = 100;
                }
          
            }
        }

    }
}

#endif // CONFIG_H