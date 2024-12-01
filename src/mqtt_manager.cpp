// mqtt_manager.cpp
#define DEBUG_LOG(msg, ...) if (Config::System::Debug::MQTT) { Serial.printf(msg "\n", ##__VA_ARGS__); }

#include "mqtt_manager.h"

/*******************************************************************************
 * Construction / Destruction
 ******************************************************************************/

MqttManager* MqttManager::instance = nullptr;

MqttManager::MqttManager(TaskManager& tm, TempSensor& ts, FanController& fc)
    : taskManager(tm)
    , tempSensor(ts)
    , fanController(fc)
    , mqttClient(wifiClient)
    , connectionMutex(nullptr)
    , messageMutex(nullptr)
    , stateMutex(nullptr)
    , messageQueue(nullptr)
    , initialized(false)
    , updateAvailable(false)
    , lastConnectAttempt(0)
    , lastClientLoop(0)
    , lastStatusUpdate(0)
    , wasConnected(false) {
    
    instance = this;
    DEBUG_LOG("Creating MQTT Manager mutexes");
    connectionMutex = xSemaphoreCreateMutex();
    messageMutex = xSemaphoreCreateMutex();
    stateMutex = xSemaphoreCreateMutex();
    messageQueue = xQueueCreate(QUEUE_SIZE, sizeof(MQTTMessage));

    if (!connectionMutex || !messageMutex || !stateMutex || !messageQueue) {
        DEBUG_LOG("Failed to create one or more mutexes/queue!");
    }
}

MqttManager::~MqttManager() {
    if (connectionMutex) vSemaphoreDelete(connectionMutex);
    if (messageMutex) vSemaphoreDelete(messageMutex);
    if (stateMutex) vSemaphoreDelete(stateMutex);
    if (messageQueue) vQueueDelete(messageQueue);
}

/*******************************************************************************
 * Core Initialization
 ******************************************************************************/

esp_err_t MqttManager::begin() {
    DEBUG_LOG("MQTT Manager Starting...");

    if (!connectionMutex || !messageMutex || !stateMutex || !messageQueue) {
        DEBUG_LOG("Resource initialization failed");
        return ESP_ERR_NO_MEM;
    }

    MutexGuard guard(connectionMutex);
    if (!guard.isLocked()) {
        DEBUG_LOG("Failed to acquire mutex in begin()");
        return ESP_ERR_TIMEOUT;
    }

    mqttClient.setServer(Config::MQTT::SERVER, Config::MQTT::PORT);
    mqttClient.setCallback(messageCallback);
    mqttClient.setSocketTimeout(30);  // 30 seconds timeout
    mqttClient.setBufferSize(1024);   // Increase buffer size
    mqttClient.setKeepAlive(60);      // 60 seconds keepalive

    // Create MQTT task using TaskManager
    TaskManager::TaskConfig taskConfig("MQTT", MQTT_STACK_SIZE, MQTT_TASK_PRIORITY, MQTT_TASK_CORE);
    DEBUG_LOG("Creating MQTT task...");
    esp_err_t err = taskManager.createTask(taskConfig, mqttTask, this);
    
    if (err != ESP_OK) {
        DEBUG_LOG("Failed to create MQTT task: %d", err);
        return err;
    }
    DEBUG_LOG("MQTT task created successfully");

    initialized = true;
    DEBUG_LOG("MQTT Manager initialized successfully");
    return ESP_OK;
}

/*******************************************************************************
 * Connection Management
 ******************************************************************************/

void MqttManager::connect() {
    DEBUG_LOG("Entering connect method");
    
    MutexGuard guard(connectionMutex);
    if (!guard.isLocked()) {
        DEBUG_LOG("Failed to acquire connection mutex in connect()");
        return;
    }

    if (connectionAttempts >= Config::MQTT::MAX_RETRIES) {
        DEBUG_LOG("Max retries reached, resetting");
        connectionAttempts = 0;
    }

    connecting = true;
    connectionAttempts++;  // Increment before attempt
    
    DEBUG_LOG("Starting connection attempt %d/%d", connectionAttempts, Config::MQTT::MAX_RETRIES);
    
    String clientId = Config::MQTT::CLIENT_ID;
    if (mqttClient.connect(clientId.c_str())) {
        DEBUG_LOG("MQTT Connected Successfully!");
        mqttClient.publish(Config::MQTT::Topics::AVAILABILITY, "online", true);
        
        if (setupSubscriptions()) {
            DEBUG_LOG("Topics Subscribed Successfully");
            wasConnected = true;
            connecting = false;
            return;
        }
    }
    
    connecting = false;
}

void MqttManager::processUpdate() {
    if (!initialized || !WiFi.isConnected()) {
        DEBUG_LOG("WiFi not connected or mqtt not initialized");
        vTaskDelay(pdMS_TO_TICKS(100));
        return;
    }

    // Handle client loop first
    unsigned long now = millis();
    
    if (now - lastClientLoop >= CLIENT_LOOP_INTERVAL) { 
        lastClientLoop = now;
        mqttClient.loop();
    }

    // Check connection state and attempt reconnection if needed
    if (!mqttClient.connected() && !connecting) {
        if (now - lastConnectAttempt >= Config::MQTT::RECONNECT_DELAY) {
            DEBUG_LOG("Attempting MQTT reconnection");
            lastConnectAttempt = now;
            connect();
        }
    }

    // Continue with normal operations if connected
    if (mqttClient.connected()) {
        // Publish availability status periodically
        if (now - lastAvailabilityPublish >= AVAILABILITY_INTERVAL) {
            lastAvailabilityPublish = now;
            mqttClient.publish(Config::MQTT::Topics::AVAILABILITY, "online", true);
            DEBUG_LOG("Published availability status");
        }

        // Process queued messages and publish status
        processQueuedMessages();
        if (now - lastStatusUpdate >= Config::MQTT::UPDATE_INTERVAL) {
            lastStatusUpdate = now;
            publishStatus();
        }
    }
}

bool MqttManager::setupSubscriptions() {
    MutexGuard guard(messageMutex);
    if (!guard.isLocked()) {
        DEBUG_LOG("Failed to acquire mutex for subscriptions");
        return false;
    }

    bool success = true;
    success &= mqttClient.subscribe(Config::MQTT::Topics::COMMAND);
    success &= mqttClient.subscribe(Config::MQTT::Topics::NIGHT_MODE);
    success &= mqttClient.subscribe(Config::MQTT::Topics::NIGHT_SETTINGS);
    success &= mqttClient.subscribe(Config::MQTT::Topics::STATE);
    success &= mqttClient.subscribe(Config::MQTT::Topics::RECOVERY);
    
    DEBUG_LOG("Subscriptions setup %s", success ? "successful" : "failed");
    return success;
}

bool MqttManager::isConnected() {
    MutexGuard guard(connectionMutex);
    if (!guard.isLocked()) return false;
    return mqttClient.connected();
}

void MqttManager::debugMutexState() {
    if (!connectionMutex) {
        DEBUG_LOG("Connection mutex is NULL!");
        return;
    }
    
    if (xSemaphoreTake(connectionMutex, pdMS_TO_TICKS(1)) == pdTRUE) {
        DEBUG_LOG("Connection mutex is available");
        xSemaphoreGive(connectionMutex);
    } else {
        DEBUG_LOG("Connection mutex is locked!");
    }
}

/*******************************************************************************
 * Message Processing
 ******************************************************************************/

void MqttManager::messageCallback(char* topic, byte* payload, unsigned int length) {
    if (!instance) {
        DEBUG_LOG("No MQTT instance available for callback");
        return;
    }
    
    DEBUG_LOG("Message received - Topic: %s, Length: %u", topic, length);
    
    if (!instance->enqueueMessage(topic, payload, length)) {
        DEBUG_LOG("Failed to enqueue MQTT message");
    }
}

bool MqttManager::enqueueMessage(const char* topic, const byte* payload, unsigned int length) {
    if (!messageQueue) {
        DEBUG_LOG("Message queue not initialized");
        return false;
    }

    MQTTMessage msg;
    strncpy(msg.topic, topic, MQTTMessage::MAX_TOPIC_LENGTH - 1);
    msg.topic[MQTTMessage::MAX_TOPIC_LENGTH - 1] = '\0';
    
    size_t copyLength = min(length, (unsigned int)(MQTTMessage::MAX_PAYLOAD_LENGTH - 1));
    memcpy(msg.payload, payload, copyLength);
    msg.payload[copyLength] = '\0';
    msg.payloadLength = copyLength;

    BaseType_t result = xQueueSend(messageQueue, &msg, pdMS_TO_TICKS(QUEUE_TIMEOUT_MS));
    DEBUG_LOG("Message enqueued: %s", result == pdTRUE ? "success" : "failed");
    return result == pdTRUE;
}

void MqttManager::processQueuedMessages() {
    const uint32_t MAX_MESSAGES_PER_CYCLE = 5;
    uint32_t processedCount = 0;  // Local counter instead of static
    MQTTMessage msg;
    
    while (processedCount < MAX_MESSAGES_PER_CYCLE && 
           xQueueReceive(messageQueue, &msg, 0) == pdTRUE) {
        DEBUG_LOG("Processing message %lu from queue - Topic: %s", 
                 processedCount, msg.topic);
        handleMessage(msg.topic, (byte*)msg.payload, msg.payloadLength);
        processedCount++;
    }
    
    if (processedCount > 0) {
        DEBUG_LOG("Processed %lu messages this cycle", processedCount);
    }
}

void MqttManager::handleMessage(const char* topic, const byte* payload, unsigned int length) {
    if (!topic || !payload || length == 0 || length >= MQTTMessage::MAX_PAYLOAD_LENGTH) {
        DEBUG_LOG("Invalid message parameters");
        return;
    }

    // Copy payload to ensure null termination
    char* payloadCopy = (char*)malloc(length + 1);
    if (!payloadCopy) {
        DEBUG_LOG("Failed to allocate memory for payload");
        return;
    }
    
    memcpy(payloadCopy, payload, length);
    payloadCopy[length] = '\0';
    
    DEBUG_LOG("Message payload: %s", payloadCopy);

    // Parse JSON payload
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payloadCopy);
    
    if (error) {
        DEBUG_LOG("JSON parsing failed: %s", error.c_str());
        free(payloadCopy);
        return;
    }

    // Process message based on topic
    {
        MutexGuard guard(messageMutex);
        if (!guard.isLocked()) {
            DEBUG_LOG("Failed to acquire mutex for message handling");
            free(payloadCopy);
            return;
        }

        if (strcmp(topic, Config::MQTT::Topics::COMMAND) == 0) {
            handleModeMessage(doc);
        }
        else if (strcmp(topic, Config::MQTT::Topics::NIGHT_MODE) == 0) {
            handleNightModeMessage(doc);
        }
        else if (strcmp(topic, Config::MQTT::Topics::RECOVERY) == 0) {
            handleRecoveryMessage(doc);
        }
        else if (strcmp(topic, Config::MQTT::Topics::NIGHT_SETTINGS) == 0) {
            handleNightSettingsMessage(doc);
        }
    }

    free(payloadCopy);
    publishStatus();
}

void MqttManager::handleNightModeMessage(const JsonDocument& doc) {
    DEBUG_LOG("Processing night mode message: %s", doc.as<String>().c_str());

    // Check if "enabled" exists and is boolean
    if (!doc["enabled"].is<bool>()) {
        DEBUG_LOG("Night mode message missing or invalid 'enabled' field");
        return;
    }

    bool enabled = doc["enabled"];
    DEBUG_LOG("Setting night mode to: %s", enabled ? "enabled" : "disabled");
    
    bool result = fanController.setNightMode(enabled);
    DEBUG_LOG("setNightMode result: %s", result ? "success" : "failed");
}

void MqttManager::handleRecoveryMessage(const JsonDocument& doc) {
    DEBUG_LOG("Processing recovery message: %s", doc.as<String>().c_str());

    if (!doc["recover"].is<bool>()) {
        DEBUG_LOG("Recovery message missing or invalid 'recover' field");
        return;
    }

    bool shouldRecover = doc["recover"];
    if (shouldRecover) {
        bool result = fanController.attemptRecovery();
        DEBUG_LOG("Recovery attempt result: %s", result ? "success" : "failed");
    }
}

void MqttManager::handleModeMessage(const JsonDocument& doc) {
    DEBUG_LOG("Processing mode message: %s", doc.as<String>().c_str());

    if (!doc["mode"].is<const char*>()) {
        DEBUG_LOG("Mode message missing or invalid 'mode' field");
        return;
    }

    const char* mode = doc["mode"];
    DEBUG_LOG("Setting mode to: %s", mode);

    if (strcmp(mode, "auto") == 0) {
        bool result = fanController.setControlMode(FanController::Mode::AUTO);
        DEBUG_LOG("Set auto mode result: %s", result ? "success" : "failed");
    }
    else if (strcmp(mode, "manual") == 0) {
        bool modeResult = fanController.setControlMode(FanController::Mode::MANUAL);
        DEBUG_LOG("Set manual mode result: %s", modeResult ? "success" : "failed");
        
        // Set speed if provided
        if (modeResult && doc["speed"].is<int>()) {
            int speed = doc["speed"];
            DEBUG_LOG("Setting manual speed to: %d", speed);
            bool speedResult = fanController.setSpeedDutyCycle(speed);
            DEBUG_LOG("Set speed result: %s", speedResult ? "success" : "failed");
        }
    }
}

void MqttManager::handleNightSettingsMessage(const JsonDocument& doc) {
    DEBUG_LOG("Processing night settings message: %s", doc.as<String>().c_str());

    // Validate required fields
    if (!doc["start_hour"].is<int>() || 
        !doc["end_hour"].is<int>() || 
        !doc["max_speed"].is<int>()) {
        DEBUG_LOG("Night settings message missing or invalid required fields");
        return;
    }

    // Get and validate values
    int startHour = doc["start_hour"];
    int endHour = doc["end_hour"];
    int maxPercent = doc["max_speed"];

    if (startHour < 0 || startHour > 23 || 
        endHour < 0 || endHour > 23 || 
        maxPercent < 0 || maxPercent > 100) {
        DEBUG_LOG("Night settings values out of range");
        return;
    }

    // Use the percentage directly with the updated FanController method
    bool result = fanController.setNightSettings(startHour, endHour, maxPercent);
    
    DEBUG_LOG("Night settings update - Start: %d, End: %d, MaxSpeed: %d%% (Result: %s)", 
              startHour, endHour, maxPercent, result ? "success" : "failed");
}

/*******************************************************************************
 * Status Publishing
 ******************************************************************************/

void MqttManager::publishString(const char* topic, const String& value) {
    MutexGuard guard(messageMutex);
    if (!guard.isLocked()) {
        DEBUG_LOG("Failed to acquire mutex for publishing");
        return;
    }

    if (mqttClient.connected()) {
        bool published = mqttClient.publish(topic, value.c_str());
        DEBUG_LOG("Published to %s: %s (success: %d)", topic, value.c_str(), published);
    } else {
        DEBUG_LOG("Failed to publish - not connected");
    }
}

void MqttManager::publishStatus() {
    MutexGuard guard(messageMutex);
    if (!guard.isLocked()) {
        DEBUG_LOG("Failed to acquire mutex for status publish");
        return;
    }

    JsonDocument doc;
    
    // Populate status document
    doc["status"] = fanController.getStatus() == FanController::Status::OK ? "ok" : "error";
    doc["fan_speed"] = fanController.getCurrentSpeed();
    doc["target_speed"] = fanController.getTargetSpeed();
    doc["rpm"] = fanController.getMeasuredRPM();
    doc["mode"] = fanController.getControlMode() == FanController::Mode::AUTO ? "auto" : "manual";
    doc["temp"] = tempSensor.getSmoothedTemp();
    doc["night_mode_enabled"] = fanController.isNightModeEnabled();
    doc["night_mode_active"] = fanController.isNightModeActive();
    doc["night_start"] = fanController.getNightStartHour();
    doc["night_end"] = fanController.getNightEndHour();
    doc["night_max_speed"] = fanController.getNightMaxSpeed();


    // Serialize and publish
    char buffer[512];
    size_t n = serializeJson(doc, buffer);
    
    if (mqttClient.connected()) {
        bool success = mqttClient.publish(Config::MQTT::Topics::COMMAND, buffer, true);
        DEBUG_LOG("Status published (success: %d): %s", success, buffer);
    } else {
        DEBUG_LOG("Cannot publish status - not connected");
    }
}

void MqttManager::publishJson(const char* topic, const JsonDocument& doc) {
    char buffer[256];
    size_t n = serializeJson(doc, buffer);
    
    if (mqttClient.connected()) {
        bool success = mqttClient.publish(topic, buffer);
        DEBUG_LOG("Published to %s (success: %d): %s", topic, success, buffer);
    }
}

/*******************************************************************************
 * Task Implementation
 ******************************************************************************/

void MqttManager::mqttTask(void* parameters) {
    MqttManager* mqtt = static_cast<MqttManager*>(parameters);
    DEBUG_LOG("MQTT Task started");
    
    while (true) {
        mqtt->taskManager.updateTaskRunTime("MQTT");
        mqtt->processUpdate();
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}

/*******************************************************************************
 * Utility Methods
 ******************************************************************************/
bool testConnection(const IPAddress& host, uint16_t port, int timeout_ms = 5000) {
    DEBUG_LOG("Testing connection to %s:%d", host.toString().c_str(), port);
    
    WiFiClient client;
    client.setTimeout(timeout_ms / 1000);
    
    unsigned long start = millis();
    bool connected = client.connect(host, port);
    unsigned long duration = millis() - start;
    
    DEBUG_LOG("Connection test took %lu ms, result: %s", 
              duration, connected ? "success" : "failed");
              
    client.stop();
    return connected;
}

String MqttManager::getMQTTStateString(int state) {
    switch (state) {
        case -4: return "MQTT_CONNECTION_TIMEOUT";
        case -3: return "MQTT_CONNECTION_LOST";
        case -2: return "MQTT_CONNECT_FAILED";
        case -1: return "MQTT_DISCONNECTED";
        case 0: return "MQTT_CONNECTED";
        case 1: return "MQTT_CONNECT_BAD_PROTOCOL";
        case 2: return "MQTT_CONNECT_BAD_CLIENT_ID";
        case 3: return "MQTT_CONNECT_UNAVAILABLE";
        case 4: return "MQTT_CONNECT_BAD_CREDENTIALS";
        case 5: return "MQTT_CONNECT_UNAUTHORIZED";
        default: return "UNKNOWN";
    }
}