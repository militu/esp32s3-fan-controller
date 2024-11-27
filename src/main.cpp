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

// System component instances
TaskManager taskManager;
WifiManager wifiManager(taskManager);
TempSensor tempSensor(taskManager);
FanController fanController(taskManager);
MqttManager mqttManager(taskManager, tempSensor, fanController);

// Display initialization based on board type
#ifdef USE_LILYGO_S3
    LilygoS3Driver display;
#else
    ILI9341Driver display(ILI9341_CS_PIN, ILI9341_DC_PIN);
#endif

DisplayManager displayManager(taskManager, tempSensor, fanController, wifiManager, mqttManager);

// Function declarations
void initializeComponents();
void performSystemHealthCheck();

/**
 * Initial setup of the ESP32 system
 * Initializes all components and verifies their status
 */
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\nESP32 System Starting...");
    
    initializeComponents();
    
    Serial.println("System initialization complete!");
}

/**
 * Main program loop
 * Handles periodic system status checks and updates
 */
void loop() {
    static uint32_t lastCheck = 0;
    uint32_t now = millis();

    // Only handle system checks here, LVGL updates are handled by dedicated task
    if (now - lastCheck >= 5000) {
        lastCheck = now;
        performSystemHealthCheck();
    }
    
    delay(1);  // Yield to other tasks
}

/**
 * Initialize all system components
 * Handles error checking and reporting for each component
 */
void initializeComponents() {
    esp_err_t err;
    
    // Initialize task manager
    err = taskManager.begin();
    if (err != ESP_OK) {
        Serial.printf("Task manager initialization failed! Error: %d\n", err);
        while (1) { delay(1000); }  // Critical failure - halt system
    }
    Serial.println("TaskManager initialized successfully");
    
    // Register component relationships
    tempSensor.registerFanController(&fanController);
    fanController.registerTempSensor(&tempSensor);

    // Initialize display
    if (!displayManager.begin(&display)) {
        Serial.println("Display initialization failed!");
    } else {
        Serial.println("Display initialized successfully");
    }

    // Initialize network and sensor components
    err = wifiManager.begin();
    if (err != ESP_OK) {
        Serial.printf("WiFi manager initialization failed! Error: %d\n", err);
    }

    err = tempSensor.begin();
    if (err != ESP_OK) {
        Serial.printf("Temperature sensor initialization failed! Error: %d\n", err);
    }

    err = fanController.begin();
    if (err != ESP_OK) {
        Serial.printf("Fan controller initialization failed! Error: %d\n", err);
    }

    err = mqttManager.begin();
    if (err != ESP_OK) {
        Serial.printf("MQTT manager initialization failed! Error: %d\n", err);
    }
}

/**
 * Perform comprehensive system health check
 * Monitors and reports status of all system components
 */
void performSystemHealthCheck() {
    // Serial.println("\n=== System Status ===");
    
    // Check task health
    bool healthy = taskManager.checkTaskHealth();
    // Serial.printf("System health: %s\n", healthy ? "OK" : "FAIL");
    if (!healthy) {
        taskManager.dumpTaskStatus();
    }

    mqttManager.debugMutexState();

    // Report WiFi status
    // Serial.printf("WiFi Status: %s\n", wifiManager.getStatusString());
    if (wifiManager.isConnected()) {
        // Serial.printf("IP: %s\n", wifiManager.getIPAddress().toString().c_str());
        // Serial.printf("Signal: %d dBm\n", wifiManager.getSignalStrength());
    }

    // Report temperature status
    // Serial.printf("Temperature Status: %s\n", tempSensor.getStatusString());
    if (tempSensor.isLastReadSuccess()) {
        // Serial.printf("Current: %.1f°C, Smoothed: %.1f°C\n",
                    // tempSensor.getCurrentTemp(),
                    // tempSensor.getSmoothedTemp());
    }

    // Report fan status
    Serial.printf("Fan Status: %s\n", fanController.getStatusString().c_str());
    Serial.printf("PWM Duty: %d%% (Target: %d%%), RPM: %d\n",
                fanController.getCurrentPWM(),
                fanController.getTargetPWM(),
                fanController.getMeasuredRPM());

    Serial.println("===================\n");

    Serial.printf("MQTT Status: %s\n", mqttManager.isConnected() ? "Connected" : "Disconnected");
}