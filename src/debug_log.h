#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <config.h>

// Create component-specific debug macros
#define DEBUG_LOG_MAIN(msg, ...) if (Config::System::Debug::MAIN) { Serial.print("[MAIN] "); Serial.printf(msg, ##__VA_ARGS__); Serial.println(); }
#define DEBUG_LOG_INIT(msg, ...) if (Config::System::Debug::INITIALIZER) { Serial.print("[INIT] "); Serial.printf(msg, ##__VA_ARGS__); Serial.println(); }
#define DEBUG_LOG_WIFI(msg, ...) if (Config::System::Debug::WIFI) { Serial.print("[WIFI] "); Serial.printf(msg, ##__VA_ARGS__); Serial.println(); }
#define DEBUG_LOG_MQTT(msg, ...) if (Config::System::Debug::MQTT) { Serial.print("[MQTT] "); Serial.printf(msg, ##__VA_ARGS__); Serial.println(); }
#define DEBUG_LOG_NTP(msg, ...) if (Config::System::Debug::NTP) { Serial.print("[NTP] "); Serial.printf(msg, ##__VA_ARGS__); Serial.println(); }
#define DEBUG_LOG_TEMP(msg, ...) if (Config::System::Debug::TEMP) { Serial.print("[TEMP] "); Serial.printf(msg, ##__VA_ARGS__); Serial.println(); }
#define DEBUG_LOG_FAN(msg, ...) if (Config::System::Debug::FAN) { Serial.print("[FAN] "); Serial.printf(msg, ##__VA_ARGS__); Serial.println(); }
#define DEBUG_LOG_DISPLAY(msg, ...) if (Config::System::Debug::SCREEN) { Serial.print("[DISP] "); Serial.printf(msg, ##__VA_ARGS__); Serial.println(); }
#define DEBUG_LOG_TASK_MANAGER(msg, ...) if (Config::System::Debug::TASK_MANAGER) { Serial.print("[TASK] "); Serial.printf(msg, ##__VA_ARGS__); Serial.println(); }
#define DEBUG_LOG_PERSISTENT(msg, ...) if (Config::System::Debug::PERSISTENT) { Serial.print("[PERS] "); Serial.printf(msg, ##__VA_ARGS__); Serial.println(); }

#endif // DEBUG_LOG_H