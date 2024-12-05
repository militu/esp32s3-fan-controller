#include <Arduino.h>
#include <OneButton.h>
#include "task_manager.h"
#include "wifi_manager.h"
#include "temp_sensor.h"
#include "fan_controller.h"
#include "config.h"
#include "mqtt_manager.h"
#include "display_manager.h"
#include "display_driver.h"
#include "system_initializer.h"
#include "debug_log.h"
#include "config_preference.h"

// System components
TaskManager taskManager;
WifiManager wifiManager(taskManager);
TempSensor tempSensor(taskManager);
NTPManager ntpManager(taskManager);
ConfigPreference configPreference;
FanController fanController(taskManager, configPreference);
MqttManager mqttManager(taskManager, tempSensor, fanController);
DisplayManager displayManager(taskManager, tempSensor, fanController, wifiManager, mqttManager);

// Button

OneButton button;
struct ButtonParams {
    DisplayDriver* driver;
    DisplayManager* displayManager;
};
static ButtonParams buttonParams;

void performSystemHealthCheck();

void setup() {
    // Set power pin first thing, important for LilyGo on battery
    #ifdef USE_LILYGO_S3
        pinMode(PIN_POWER_ON, OUTPUT);
        digitalWrite(PIN_POWER_ON, HIGH);
    #endif

    Serial.begin(115200);
    delay(1000);
    Serial.println("\nESP32 System Starting...");

    if (!configPreference.begin()) {
        Serial.println("Config preference initialization failed!");
        return;
    }

    DisplayDriver* displayDriver = createDisplayDriver();
    if (!displayDriver) {
        Serial.println("Failed to create display driver!");
        return;
    }

    // Initialize first
    SystemInitializer initializer(
        taskManager, displayManager, displayDriver,
        wifiManager, ntpManager, mqttManager,
        tempSensor, fanController, configPreference
    );

    SystemInitializer::InitConfig config(false); // false = Perform network initialization

    if (!initializer.initialize(config)) {
        Serial.println("System initialization failed!");
        return;
    }

    // Add a delay before button setup
    delay(100);
 
    // Initialize the button params after creating components
    buttonParams = ButtonParams{displayDriver, &displayManager};

    // Setup button with the static params
    button.setup(Config::Hardware::PIN_BUTTON_1, INPUT_PULLUP, true);
    button.setDebounceMs(50);
    button.setClickMs(50);

    button.attachClick([](void* ctx) {
        Serial.println("Button Press Detected");
        auto* params = static_cast<ButtonParams*>(ctx);
        params->displayManager->handleButtonPress();
    }, &buttonParams);

    Serial.println("System initialization complete!");
}

void loop() {
    button.tick(); // Call this in the loop to process button events

    static uint32_t lastCheck = 0;
    uint32_t now = millis();

    if (now - lastCheck >= 5000) {
        lastCheck = now;
        performSystemHealthCheck();
    }
    
    delay(1);
}

void performSystemHealthCheck() {
    DEBUG_LOG_MAIN("\n=== System Status ===");

    // Check task health
    bool healthy = taskManager.checkTaskHealth();
    DEBUG_LOG_MAIN("System health: %s", healthy ? "OK" : "FAIL");
    if (!healthy) {
        taskManager.dumpTaskStatus();
    }

    // Report WiFi status
    DEBUG_LOG_MAIN("WiFi Status: %s", wifiManager.getStatusString());
    if (wifiManager.isConnected()) {
        DEBUG_LOG_MAIN("IP: %s", wifiManager.getIPAddress().toString().c_str());
        DEBUG_LOG_MAIN("Signal: %d dBm", wifiManager.getSignalStrength());
    }

    // Report temperature status
    DEBUG_LOG_MAIN("Temperature Status: %s", tempSensor.getStatusString());
    if (tempSensor.isLastReadSuccess()) {
        DEBUG_LOG_MAIN("Current: %.1f°C, Smoothed: %.1f°C",
                       tempSensor.getCurrentTemp(),
                       tempSensor.getSmoothedTemp());
    }

    // Report fan status
    DEBUG_LOG_MAIN("Fan Status: %s", fanController.getStatusString().c_str());
    DEBUG_LOG_MAIN("Speed: %d%% (Target: %d%%), RPM: %d",
                   fanController.getCurrentSpeed(),
                   fanController.getTargetSpeed(),
                   fanController.getMeasuredRPM());

    // Report network service status
    DEBUG_LOG_MAIN("MQTT Status: %s", 
                   mqttManager.isConnected() ? "Connected" : "Disconnected");
    DEBUG_LOG_MAIN("NTP Status: %s", 
                   ntpManager.isTimeSynchronized() ? 
                   ("Synchronized - " + ntpManager.getTimeString()).c_str() : 
                   "Not synchronized");

    DEBUG_LOG_MAIN("===================\n");
}