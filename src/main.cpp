/**
 * ESP32-based Environmental Control System
 * 
 * This system manages temperature monitoring, fan control, display output,
 * and MQTT communication for an environmental control application.
 */

#include <Arduino.h>
#include "task_manager.h"
#include "wifi_manager.h"
#include "temp_sensor.h"
#include "fan_controller.h"
#include "config.h"
#include "mqtt_manager.h"
#include "display_manager.h"
#include "display_driver.h"
#include "display_config.h"
#include "system_initializer.h"
#include "debug_log.h"

// System component instances
TaskManager taskManager;
WifiManager wifiManager(taskManager);
TempSensor tempSensor(taskManager);
FanController fanController(taskManager);
MqttManager mqttManager(taskManager, tempSensor, fanController);
NTPManager ntpManager(taskManager);

// Display initialization based on board type
#ifdef USE_LILYGO_S3
    LilygoS3Driver display;
#else
    ILI9341Driver display(ILI9341_CS_PIN, ILI9341_DC_PIN);
#endif

DisplayManager displayManager(taskManager, tempSensor, fanController, wifiManager, mqttManager);
void performSystemHealthCheck();

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\nESP32 System Starting...");

    SystemInitializer initializer(
        taskManager, displayManager, &display, 
        wifiManager, ntpManager, mqttManager,
        tempSensor, fanController
    );
    
    if (!initializer.initialize()) {
        Serial.println("System initialization failed!");
        return;
    }
    Serial.println("System initialization complete!");
}

void loop() {
    static uint32_t lastCheck = 0;
    uint32_t now = millis();

    if (now - lastCheck >= 5000) {
        lastCheck = now;
        performSystemHealthCheck();
    }
    
    delay(1);
}

/**
 * Perform comprehensive system health check
 * Monitors and reports status of all system components
 */
void performSystemHealthCheck() {
    DEBUG_LOG_MAIN("\n=== System Status ===");
    
    // Check task health
    bool healthy = taskManager.checkTaskHealth();
    DEBUG_LOG_MAIN("System health: %s", healthy ? "OK" : "FAIL");
    if (!healthy) {
        taskManager.dumpTaskStatus();
    }

    mqttManager.debugMutexState();

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