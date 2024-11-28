/**
 * @file mqtt_manager.h
 * @brief MQTT communication manager for IoT device control
 * 
 * Handles MQTT protocol communication including connection management,
 * message processing, state synchronization, and device control.
 */

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "config.h"
#include "task_manager.h"
#include "temp_sensor.h"
#include "fan_controller.h"
#include "mutex_guard.h"

class MqttManager {
public:
    /**
     * @brief Constructor
     * @param taskManager Task management system
     * @param tempSensor Temperature sensor interface
     * @param fanController Fan control interface
     */
    explicit MqttManager(TaskManager& taskManager, TempSensor& tempSensor, FanController& fanController);
    
    /**
     * @brief Destructor - cleanup resources
     */
    ~MqttManager();

    // Prevent copying
    MqttManager(const MqttManager&) = delete;
    MqttManager& operator=(const MqttManager&) = delete;

    /**
     * @brief Initialize the MQTT manager
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t begin();

    /**
     * @brief Check connection status
     * @return true if connected to broker
     */
    bool isConnected();

    /**
     * @brief Check initialization status
     * @return true if manager is initialized
     */
    bool isInitialized() const { return initialized; }

    /**
     * @brief Debug mutex state
     */
    void debugMutexState();

    struct ConnectionState {
        bool connecting;
        uint8_t currentAttempt;
        bool connected;
    };

    ConnectionState getConnectionState() {
        MutexGuard guard(connectionMutex);
        if (!guard.isLocked()) {
            return {false, 0, false};
        }
        
        return ConnectionState{
            connecting,
            connectionAttempts,  // Use directly, no need for +1 here
            mqttClient.connected()
        };
    }

private:
    // Constants
    static constexpr size_t MAX_TOPIC_LENGTH = 64;
    static constexpr size_t MAX_PAYLOAD_LENGTH = 256;
    static constexpr size_t QUEUE_SIZE = 10;
    static constexpr uint32_t QUEUE_TIMEOUT_MS = 100;
    static constexpr uint32_t MUTEX_TIMEOUT_MS = 1000;
    static constexpr uint32_t MQTT_STACK_SIZE = 8192;
    static constexpr UBaseType_t MQTT_TASK_PRIORITY = 3;
    static constexpr BaseType_t MQTT_TASK_CORE = 1;
    static constexpr unsigned long AVAILABILITY_INTERVAL = 30000;  // 30 seconds
    static constexpr uint32_t CLIENT_LOOP_INTERVAL = 50; 

    // Message structure
    struct MQTTMessage {
        char topic[MAX_TOPIC_LENGTH];
        char payload[MAX_PAYLOAD_LENGTH];
        size_t payloadLength;
    };

    // Core components
    TaskManager& taskManager;
    TempSensor& tempSensor;
    FanController& fanController;
    WiFiClient wifiClient;
    PubSubClient mqttClient;

    // Synchronization primitives
    SemaphoreHandle_t connectionMutex;
    SemaphoreHandle_t messageMutex;
    SemaphoreHandle_t stateMutex;
    QueueHandle_t messageQueue;

    // State tracking
    bool initialized;
    bool updateAvailable;
    bool wasConnected;
    static MqttManager* instance;

    // Timing variables
    unsigned long lastConnectAttempt;
    unsigned long lastClientLoop;
    unsigned long lastStatusUpdate;
    unsigned long lastAvailabilityPublish;

    // Connection management
    void connect();
    bool setupSubscriptions();
    void processUpdate();
    void publishStatus();

    // Message handling
    static void messageCallback(char* topic, byte* payload, unsigned int length);
    void handleMessage(const char* topic, const byte* payload, unsigned int length);
    void handleModeMessage(const JsonDocument& doc);
    void handleNightModeMessage(const JsonDocument& doc);
    void handleRecoveryMessage(const JsonDocument& doc);
    void handleNightSettingsMessage(const JsonDocument& doc);
    void processQueuedMessages();
    bool enqueueMessage(const char* topic, const byte* payload, unsigned int length);
    
    // Publication methods
    void publishString(const char* topic, const String& value);
    void publishJson(const char* topic, const JsonDocument& doc);

    // Utility functions
    String getMQTTStateString(int state);
    
    // Task management
    static void mqttTask(void* parameters);

    uint8_t connectionAttempts;
    bool connecting;
};