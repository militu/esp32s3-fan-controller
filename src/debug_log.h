#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <config.h>

// Create component-specific debug macros
#define DEBUG_LOG_MAIN(msg, ...) if (DEBUG_MAIN) { Serial.printf(msg "\n", ##__VA_ARGS__); }
#define DEBUG_LOG_INIT(msg, ...) if (DEBUG_INITIALIZER) { Serial.printf(msg "\n", ##__VA_ARGS__); }
#define DEBUG_LOG_WIFI(msg, ...) if (DEBUG_WIFI) { Serial.printf(msg "\n", ##__VA_ARGS__); }
#define DEBUG_LOG_MQTT(msg, ...) if (DEBUG_MQTT) { Serial.printf(msg "\n", ##__VA_ARGS__); }
#define DEBUG_LOG_NTP(msg, ...) if (DEBUG_NTP) { Serial.printf(msg "\n", ##__VA_ARGS__); }
#define DEBUG_LOG_TEMP(msg, ...) if (DEBUG_TEMP) { Serial.printf(msg "\n", ##__VA_ARGS__); }
#define DEBUG_LOG_FAN(msg, ...) if (DEBUG_FAN) { Serial.printf(msg "\n", ##__VA_ARGS__); }
#define DEBUG_LOG_DISPLAY(msg, ...) if (DEBUG_DISPLAY) { Serial.printf(msg "\n", ##__VA_ARGS__); }
#define DEBUG_LOG_TM(msg, ...) if (DEBUG_TM) { Serial.printf(msg "\n", ##__VA_ARGS__); }
#define DEBUG_LOG_PERSISTENT(msg, ...) if (DEBUG_PERSISTENT) { Serial.printf(msg "\n", ##__VA_ARGS__); }

#endif // DEBUG_LOG_H