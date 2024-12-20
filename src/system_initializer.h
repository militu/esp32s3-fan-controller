#include "debug_log.h"

// system_initializer.h
class SystemInitializer {
public:
    struct InitConfig {
        bool skipNetworking;  // If true, skips WiFi, NTP, and MQTT initialization

        InitConfig(bool skip = false) : skipNetworking(skip) {}
    };

    SystemInitializer(TaskManager& tasks,
                     DisplayManager& display,
                     DisplayDriver* driver,
                     WifiManager& wifi,
                     NTPManager& ntp,
                     MqttManager& mqtt,
                     TempSensor& temp,
                     FanController& fan,
                     ConfigPreference& config)
        : taskManager(tasks)
        , displayManager(display)
        , displayDriver(driver)
        , wifiManager(wifi)
        , ntpManager(ntp)
        , mqttManager(mqtt)
        , tempSensor(temp)
        , fanController(fan)
        , configPreference(config) {}

bool initialize(const InitConfig& config = InitConfig()) {
    // Critical components must succeed
    if (!initializeCriticalComponents()) {
        DEBUG_LOG_INIT("Critical components initialization failed");
        return false;
    }
    
    // Operational components must succeed
    if (!initializeOperationalComponents()) {
        DEBUG_LOG_INIT("Operational components initialization failed");
        return false;
    }

    // Skip networking components if in test mode
    if (!config.skipNetworking) {
        // Network components - try in correct order but continue if they fail
        bool wifiSuccess = initializeWifi();
        
        // Only try NTP and MQTT if WiFi succeeded
        bool ntpSuccess = false;
        bool mqttSuccess = false;
        
        if (wifiSuccess) {
            ntpSuccess = initializeNTP();
            mqttSuccess = initializeMQTT();
        } else {
            DEBUG_LOG_INIT("Skipping NTP and MQTT initialization due to WiFi failure");
        }

        DEBUG_LOG_INIT("Initialization complete - WiFi: %d, NTP: %d, MQTT: %d", 
                    wifiSuccess, ntpSuccess, mqttSuccess);
        delay(1000);
    } else {
        DEBUG_LOG_INIT("Test mode: Skipping network initialization");
        delay(2000);
    }

    // Switch to dashboard screen after initialization attempts
    displayManager.switchToDashboardUI();
    
    // Always return true after critical and operational components are initialized
    return true;
}

private:
    TaskManager& taskManager;
    DisplayManager& displayManager;
    DisplayDriver* displayDriver;
    WifiManager& wifiManager;
    NTPManager& ntpManager;
    MqttManager& mqttManager;
    TempSensor& tempSensor;
    FanController& fanController;
    ConfigPreference& configPreference;

    bool initializeCriticalComponents() {
        // Initialize task manager
        if (taskManager.begin() != ESP_OK) {
            DEBUG_LOG_INIT("Task manager initialization failed!");
            return false;
        }

        // Initialize display
        if (!displayManager.begin(displayDriver)) {
            DEBUG_LOG_INIT("Display initialization failed!");
            return false;
        }

        return true;
    }

    bool initializeOperationalComponents() {
        // Register component relationships
        tempSensor.registerFanController(&fanController);
        fanController.registerTempSensor(&tempSensor);
        fanController.registerNTPManager(&ntpManager);

        // Initialize temperature sensor
        if (tempSensor.begin() != ESP_OK) {
            DEBUG_LOG_INIT("Temperature sensor initialization failed!");
            return false;
        }

        // Initialize fan controller
        if (fanController.begin() != ESP_OK) {
            DEBUG_LOG_INIT("Fan controller initialization failed!");
            return false;
        }
        fanController.loadSettings(configPreference);


        return true;
    }

    bool initializeWifi() {
        displayManager.showWifiInitializing();
        
        if (wifiManager.begin() != ESP_OK) {
            displayManager.showWifiFailed("Initialization failed");
            return false;
        }

        uint32_t startTime = millis();
        uint8_t lastAttempt = 0;  // Track last seen attempt
        uint32_t wifiTimeout = wifiManager.getTotalTimeout(); 

        while (!wifiManager.isConnected()) {
            if (millis() - startTime > wifiTimeout) {
                displayManager.showWifiFailed("Connection timeout");
                return false;
            }

            // Only update display if attempt number has changed
            uint8_t currentAttempt = wifiManager.getCurrentAttempt();
            if (currentAttempt != lastAttempt) {
                lastAttempt = currentAttempt;
                displayManager.showWifiConnecting(currentAttempt, Config::WiFi::MAX_RETRIES);
            }
            
            delay(100);
        }

        displayManager.showWifiConnected(Config::WiFi::SSID, wifiManager.getIPAddress());
        return true;
    }

    bool initializeNTP() {
        displayManager.showNTPInitializing();
        
        if (ntpManager.begin() != ESP_OK) {
            displayManager.showNTPFailed("Initialization failed");
            return false;
        }

        uint32_t startTime = millis();
        uint8_t lastAttempt = 0;  // Track last seen attempt

        while (!ntpManager.isTimeSynchronized()) {
            if (ntpManager.getCurrentAttempt() >= Config::NTP::MAX_SYNC_ATTEMPTS) {
                displayManager.showNTPFailed("Max attempts reached");
                return false;
            }

            // Check for timeout
            if (millis() - startTime > 30000) {
                displayManager.showNTPFailed("Initialization timeout");
                return false;
            }

            // Only update display if attempt number has changed
            uint8_t currentAttempt = ntpManager.getCurrentAttempt();
            if (currentAttempt != lastAttempt) {
                lastAttempt = currentAttempt;
                displayManager.showNTPSyncing(
                    currentAttempt,
                    Config::NTP::MAX_SYNC_ATTEMPTS
                );
            }

            // Small delay to prevent tight loop
            delay(100);
        }

        // If we get here, synchronization was successful
        displayManager.showNTPSynced(ntpManager.getTimeString());
        return true;
    }
    bool initializeMQTT() {
        displayManager.showMQTTInitializing();
        
        if (mqttManager.begin() != ESP_OK) {
            displayManager.showMQTTFailed("Initialization failed");
            return false;
        }

        uint32_t startTime = millis();
        uint8_t lastAttempt = 0;
        uint32_t mqttTimeout = mqttManager.getTotalTimeout(); 

        while (millis() - startTime < mqttTimeout) {
            auto state = mqttManager.getConnectionState();
            
            // Update display when attempt number changes
            if (state.currentAttempt != lastAttempt) {
                lastAttempt = state.currentAttempt;
                if (state.currentAttempt > 0) {  // Only show if we have an actual attempt
                    displayManager.showMQTTConnecting(
                        state.currentAttempt,
                        Config::MQTT::MAX_RETRIES
                    );
                }
            }
            
            if (state.connected || mqttManager.isConnected()) {
                displayManager.showMQTTConnected();
                return true;
            }
            
            delay(1000);
        }

        displayManager.showMQTTFailed("Connection timeout");
        return false;
    }
};