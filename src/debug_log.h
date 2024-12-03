#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <config.h>

// Create component-specific debug macros
#define DEBUG_LOG_MAIN(msg, ...) if (Config::System::Debug::MAIN) { Serial.printf(msg "\n", ##__VA_ARGS__); }
#define DEBUG_LOG_INIT(msg, ...) if (Config::System::Debug::INITIALIZER) { Serial.printf(msg "\n", ##__VA_ARGS__); }
#define DEBUG_LOG_WIFI(msg, ...) if (Config::System::Debug::WIFI) { Serial.printf(msg "\n", ##__VA_ARGS__); }
#define DEBUG_LOG_MQTT(msg, ...) if (Config::System::Debug::MQTT) { Serial.printf(msg "\n", ##__VA_ARGS__); }
#define DEBUG_LOG_NTP(msg, ...) if (Config::System::Debug::NTP) { Serial.printf(msg "\n", ##__VA_ARGS__); }
#define DEBUG_LOG_TEMP(msg, ...) if (Config::System::Debug::TEMP) { Serial.printf(msg "\n", ##__VA_ARGS__); }
#define DEBUG_LOG_FAN(msg, ...) if (Config::System::Debug::FAN) { Serial.printf(msg "\n", ##__VA_ARGS__); }
#define DEBUG_LOG_DISPLAY(msg, ...) if (Config::System::Debug::SCREEN) { Serial.printf(msg "\n", ##__VA_ARGS__); }
#define DEBUG_LOG_TASK_MANAGER(msg, ...) if (Config::System::Debug::TASK_MANAGER) { Serial.printf(msg "\n", ##__VA_ARGS__); }
#define DEBUG_LOG_PERSISTENT(msg, ...) if (Config::System::Debug::PERSISTENT) { Serial.printf(msg "\n", ##__VA_ARGS__); }

#endif // DEBUG_LOG_H