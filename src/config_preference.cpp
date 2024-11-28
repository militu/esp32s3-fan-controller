#include "config_preference.h"
#include "fan_controller.h"
#include "debug_log.h"

ConfigPreference::ConfigPreference() 
    : mutex(xSemaphoreCreateMutex())
    , initialized(false) {
}

ConfigPreference::~ConfigPreference() {
    if (mutex) {
        vSemaphoreDelete(mutex);
    }
    prefs.end();
}

bool ConfigPreference::begin() {
    MutexGuard guard(mutex);
    if (!guard.isLocked()) return false;

    if (initialized) return true;

    if (!prefs.begin("fanprefs", false)) {
        return false;
    }

    initialized = true;
    return true;
}

bool ConfigPreference::saveFanSettings(const FanSettings& settings) {
    MutexGuard guard(mutex);
    if (!guard.isLocked() || !initialized) return false;

    // Don't save if in ERROR mode (2 is ERROR in the enum)
    if (settings.fanMode == 2) {
        return false;
    }

    prefs.putUChar("fanMode", settings.fanMode);
    DEBUG_LOG_PERSISTENT("SAVE CONFIG: FanMode=%d\n", settings.fanMode);
    DEBUG_LOG_PERSISTENT("SAVE CONFIG: ManSpeed=%d\n", settings.manualSpeed);
    DEBUG_LOG_PERSISTENT("SAVE CONFIG: NightMode=%d\n", settings.nightModeEnabled);

    // Rest of the save operations...
    prefs.putUChar("manSpeed", settings.manualSpeed);
    prefs.putBool("nightMode", settings.nightModeEnabled);
    prefs.putUChar("nightStart", settings.nightStartHour);
    prefs.putUChar("nightEnd", settings.nightEndHour);
    prefs.putUChar("nightSpeed", settings.nightMaxSpeed);

    return true;
}

bool ConfigPreference::loadFanSettings(FanSettings& settings) {
    MutexGuard guard(mutex);
    if (!guard.isLocked() || !initialized) {
        setDefaultFanSettings(settings);
        return false;
    }

    settings.fanMode = prefs.getUChar("fanMode", 0);  // 0 = AUTO mode
    DEBUG_LOG_PERSISTENT("LOAD CONFIG: FanMode=%d\n", settings.fanMode);
    
    settings.manualSpeed = prefs.getUChar("manSpeed", FAN_MIN_SPEED);
    DEBUG_LOG_PERSISTENT("LOAD CONFIG: ManSpeed=%d\n", settings.manualSpeed);
    
    settings.nightModeEnabled = prefs.getBool("nightMode", false);
    DEBUG_LOG_PERSISTENT("LOAD CONFIG: NightMode=%d\n", settings.nightModeEnabled);

    // Rest of the load operations...
    if (settings.fanMode > 1) {  // If ERROR or invalid, set to AUTO
        settings.fanMode = 0;
    }
    settings.nightStartHour = prefs.getUChar("nightStart", NIGHT_MODE_START);
    settings.nightEndHour = prefs.getUChar("nightEnd", NIGHT_MODE_END);
    settings.nightMaxSpeed = prefs.getUChar("nightSpeed", NIGHT_MODE_MAX_SPEED);

    return true;
}

void ConfigPreference::setDefaultFanSettings(FanSettings& settings) {
    settings.fanMode = 0;  // AUTO mode
    settings.manualSpeed = FAN_MIN_SPEED;
    settings.nightModeEnabled = false;
    settings.nightStartHour = NIGHT_MODE_START;
    settings.nightEndHour = NIGHT_MODE_END;
    settings.nightMaxSpeed = NIGHT_MODE_MAX_SPEED;
}

bool ConfigPreference::resetToDefaults() {
    MutexGuard guard(mutex);
    if (!guard.isLocked() || !initialized) return false;

    return prefs.clear();
}