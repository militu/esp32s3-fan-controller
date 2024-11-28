#ifndef CONFIG_PREFERENCE_H
#define CONFIG_PREFERENCE_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include "mutex_guard.h"

// Forward declaration
class FanController;

class ConfigPreference {
public:
    // Use uint8_t for mode to avoid circular dependency
    struct FanSettings {
        uint8_t fanMode;  // Will store static_cast of FanController::Mode
        uint8_t manualSpeed;
        bool nightModeEnabled;
        uint8_t nightStartHour;
        uint8_t nightEndHour;
        uint8_t nightMaxSpeed;
    };

    ConfigPreference();
    ~ConfigPreference();

    bool begin();
    bool saveFanSettings(const FanSettings& settings);
    bool loadFanSettings(FanSettings& settings);
    bool resetToDefaults();

private:
    Preferences prefs;
    SemaphoreHandle_t mutex;
    bool initialized;

    void setDefaultFanSettings(FanSettings& settings);
};

#endif // CONFIG_PREFERENCE_H