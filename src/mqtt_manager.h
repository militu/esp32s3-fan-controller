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

/**
 * @brief MQTT communication manager for IoT device control
 * 
 * Features:
 * - Automatic connection and reconnection to MQTT broker
 * - Message queueing and processing
 * - JSON payload handling
 * - Thread-safe operation
 * - Status publishing and command handling
 */
class MqttManager {
public:
    /**
     * @brief Message structure for MQTT queue
     */
    struct MQTTMessage {        
        char topic[Config::MQTT::Message::MAX_TOPIC_LENGTH];
        char payload[Config::MQTT::Message::MAX_PAYLOAD_LENGTH];
        size_t payloadLength;
    };

    enum class MessageAction {
        INVALID,
        MODE,
        NIGHT_MODE,
        RECOVERY,
        NIGHT_SETTINGS
    };

    /**
     * @brief Connection state information
     */
    struct ConnectionState {
        bool connecting;
        uint8_t currentAttempt;
        bool connected;
    };

    // Constructor and destructor
    explicit MqttManager(TaskManager& taskManager, 
                        TempSensor& tempSensor, 
                        FanController& fanController);
    ~MqttManager();

    // Prevent copying
    MqttManager(const MqttManager&) = delete;
    MqttManager& operator=(const MqttManager&) = delete;

    // Core functionality
    esp_err_t begin();
    bool isConnected();
    bool isInitialized() const { return initialized; }
    void debugMutexState();

    // Connection state query
    ConnectionState getConnectionState() {
        
        return ConnectionState{
            connecting,
            connectionAttempts,
            mqttClient.connected()
        };
    }

    uint32_t getTotalTimeout();

private:
    // Core components
    TaskManager& taskManager;
    TempSensor& tempSensor;
    FanController& fanController;
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    static MqttManager* instance;

    // Synchronization primitives
    SemaphoreHandle_t connectionMutex;
    SemaphoreHandle_t messageMutex;
    SemaphoreHandle_t stateMutex;
    QueueHandle_t messageQueue;

    // State tracking
    bool initialized;
    bool updateAvailable;
    bool wasConnected;
    bool connecting;
    uint8_t connectionAttempts;

    // Timing variables
    uint32_t lastConnectAttempt;
    uint32_t lastClientLoop;
    uint32_t lastStatusUpdate;
    uint32_t lastAvailabilityPublish;

    // Connection management methods
    void connect();
    bool setupSubscriptions();
    void processUpdate();
    void publishStatus();

    // Message handling methods
    static void messageCallback(char* topic, byte* payload, unsigned int length);
    void handleMessage(const char* topic, const byte* payload, unsigned int length);
    bool handleModeMessage(const JsonDocument& doc);
    bool handleNightModeMessage(const JsonDocument& doc);
    bool handleRecoveryMessage(const JsonDocument& doc);
    bool handleNightSettingsMessage(const JsonDocument& doc);
    void processQueuedMessages();
    bool enqueueMessage(const char* topic, const byte* payload, unsigned int length);
    MessageAction determineMessageAction(const char* topic);

    // Publication methods
    void publishString(const char* topic, const String& value);
    bool publishJson(const char* topic, const JsonDocument& doc);
    const char* getFanStatusString(FanController::Status status);

    // Utility methods
    String getMQTTStateString(int state);
    static void mqttTask(void* parameters);
};