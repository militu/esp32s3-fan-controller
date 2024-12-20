// mqtt_manager.cpp
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
    DEBUG_LOG_MQTT("Creating MQTT Manager mutexes");
    connectionMutex = xSemaphoreCreateMutex();
    messageMutex = xSemaphoreCreateMutex();
    stateMutex = xSemaphoreCreateMutex();
    messageQueue = xQueueCreate(Config::MQTT::QUEUE_SIZE, sizeof(MQTTMessage));

    if (!connectionMutex || !messageMutex || !stateMutex || !messageQueue) {
        DEBUG_LOG_MQTT("Failed to create one or more mutexes/queue!");
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
    DEBUG_LOG_MQTT("MQTT Manager Starting...");

    if (!connectionMutex || !messageMutex || !stateMutex || !messageQueue) {
        DEBUG_LOG_MQTT("Resource initialization failed");
        return ESP_ERR_NO_MEM;
    }

    MutexGuard guard(connectionMutex);
    if (!guard.isLocked()) {
        DEBUG_LOG_MQTT("Failed to acquire mutex in begin()");
        return ESP_ERR_TIMEOUT;
    }

    mqttClient.setServer(Config::MQTT::SERVER, Config::MQTT::PORT);
    mqttClient.setCallback(messageCallback);
    mqttClient.setSocketTimeout(5);  // 5 seconds timeout
    mqttClient.setBufferSize(512);   // Increase buffer size
    mqttClient.setKeepAlive(30);      // 30 seconds keepalive

    // Create MQTT task using TaskManager
    TaskManager::TaskConfig taskConfig("MQTT", 
                                       Config::MQTT::Task::STACK_SIZE, 
                                       Config::MQTT::Task::TASK_PRIORITY, 
                                       Config::MQTT::Task::TASK_CORE);
    DEBUG_LOG_MQTT("Creating MQTT task...");
    esp_err_t err = taskManager.createTask(taskConfig, mqttTask, this);
    
    if (err != ESP_OK) {
        DEBUG_LOG_MQTT("Failed to create MQTT task: %d", err);
        return err;
    }
    DEBUG_LOG_MQTT("MQTT task created successfully");

    initialized = true;
    DEBUG_LOG_MQTT("MQTT Manager initialized successfully");
    return ESP_OK;
}

/*******************************************************************************
 * Connection Management
 ******************************************************************************/

void MqttManager::connect() {
    DEBUG_LOG_MQTT("Entering connect method");
    
    MutexGuard guard(connectionMutex);
    if (!guard.isLocked()) {
        DEBUG_LOG_MQTT("Failed to acquire connection mutex in connect()");
        return;
    }

    if (connectionAttempts >= Config::MQTT::MAX_RETRIES) {
        DEBUG_LOG_MQTT("Max retries reached, resetting");
        connectionAttempts = 0;
        connecting = false;
        return;
    }

    connecting = true;
    connectionAttempts++;  // Increment before attempt

    // Calculate backoff delay based on attempt number (with proper type handling)
    uint32_t baseDelay = Config::MQTT::RECONNECT_DELAY_MS;
    uint32_t maxDelay = 30000UL;
    
    // Cast to uint32_t and limit the shift value directly without min
    uint32_t shiftAmount = (connectionAttempts > 5) ? 4U : (uint32_t)(connectionAttempts - 1);
    uint32_t backoffDelay = baseDelay * (1U << shiftAmount);
    
    DEBUG_LOG_MQTT("Starting connection attempt %d/%d", connectionAttempts, Config::MQTT::MAX_RETRIES);

    delay(backoffDelay);  // Apply backoff delay

    String clientId = String(Config::MQTT::CLIENT_ID) + String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str(), Config::MQTT::USERNAME, Config::MQTT::PASSWORD, 
                          Config::MQTT::Topics::AVAILABILITY,
                          0, true, "offline")) {
        DEBUG_LOG_MQTT("MQTT Connected Successfully!");
        mqttClient.publish(Config::MQTT::Topics::AVAILABILITY, "online", true);
        
        if (setupSubscriptions()) {
            wasConnected = true;
            connecting = false;
            publishStatus();
            return;
        }
    }
    
    connecting = false;
}

void MqttManager::processUpdate() {
    if (!initialized || !WiFi.isConnected()) {
        DEBUG_LOG_MQTT("WiFi not connected or mqtt not initialized");
        vTaskDelay(pdMS_TO_TICKS(100));
        return;
    }
    unsigned long now = millis();

    // Handle client loop and basic MQTT operations
    if (now - lastClientLoop >= Config::MQTT::CLIENT_LOOP_INTERVAL) { 
        lastClientLoop = now;
        mqttClient.loop();
    }

    // Check connection and reconnect if needed
    if (!mqttClient.connected() && !connecting) {
        if (now - lastConnectAttempt >= Config::MQTT::RECONNECT_DELAY_MS) {
            DEBUG_LOG_MQTT("Attempting MQTT reconnection");
            lastConnectAttempt = now;
            connect();
        }
        return;
    }

    // Connected state operations
    if (mqttClient.connected()) {
        connectionAttempts = 0;

        // Publish availability status periodically
        if (now - lastAvailabilityPublish >= Config::MQTT::AVAILABILITY_INTERVAL) {
            lastAvailabilityPublish = now;
            mqttClient.publish(Config::MQTT::Topics::AVAILABILITY, "online", true);
            DEBUG_LOG_MQTT("Published availability status");
        }

        // Process queued messages
        processQueuedMessages();
        
        // Update status periodically
        if (now - lastStatusUpdate >= Config::MQTT::UPDATE_INTERVAL) {
            lastStatusUpdate = now;
            publishStatus();
        }
    }
}

bool MqttManager::setupSubscriptions() {
    MutexGuard guard(messageMutex);
    if (!guard.isLocked()) {
        DEBUG_LOG_MQTT("Failed to acquire mutex for subscriptions");
        return false;
    }

    bool success = true;
    
    // Control topics
    success &= mqttClient.subscribe(Config::MQTT::Topics::Control::MODE);
    success &= mqttClient.subscribe(Config::MQTT::Topics::Control::NIGHT_MODE);
    success &= mqttClient.subscribe(Config::MQTT::Topics::Control::NIGHT_SETTINGS);
    success &= mqttClient.subscribe(Config::MQTT::Topics::Control::RECOVERY);
    
    DEBUG_LOG_MQTT("Subscriptions setup %s", success ? "successful" : "failed");
    return success;
}

bool MqttManager::isConnected() {
    MutexGuard guard(connectionMutex);
    if (!guard.isLocked()) return false;
    return mqttClient.connected();
}

void MqttManager::debugMutexState() {
    if (!connectionMutex) {
        DEBUG_LOG_MQTT("Connection mutex is NULL!");
        return;
    }
    
    if (xSemaphoreTake(connectionMutex, pdMS_TO_TICKS(1)) == pdTRUE) {
        DEBUG_LOG_MQTT("Connection mutex is available");
        xSemaphoreGive(connectionMutex);
    } else {
        DEBUG_LOG_MQTT("Connection mutex is locked!");
    }
}

/*******************************************************************************
 * Message Processing
 ******************************************************************************/

void MqttManager::messageCallback(char* topic, byte* payload, unsigned int length) {
    if (!instance) {
        DEBUG_LOG_MQTT("No MQTT instance available for callback");
        return;
    }
    
    DEBUG_LOG_MQTT("Message received - Topic: %s, Length: %u", topic, length);
    
    if (!instance->enqueueMessage(topic, payload, length)) {
        DEBUG_LOG_MQTT("Failed to enqueue MQTT message");
    }
}

bool MqttManager::enqueueMessage(const char* topic, const byte* payload, unsigned int length) {
    if (!messageQueue) {
        DEBUG_LOG_MQTT("Message queue not initialized");
        return false;
    }

    MQTTMessage msg;
    strncpy(msg.topic, topic, Config::MQTT::Message::MAX_TOPIC_LENGTH - 1);
    msg.topic[Config::MQTT::Message::MAX_TOPIC_LENGTH - 1] = '\0';
    
    size_t copyLength = min(length, (unsigned int)(Config::MQTT::Message::MAX_PAYLOAD_LENGTH - 1));
    memcpy(msg.payload, payload, copyLength);
    msg.payload[copyLength] = '\0';
    msg.payloadLength = copyLength;

    BaseType_t result = xQueueSend(messageQueue, &msg, pdMS_TO_TICKS(Config::MQTT::QUEUE_TIMEOUT_MS));
    DEBUG_LOG_MQTT("Message enqueued: %s", result == pdTRUE ? "success" : "failed");
    return result == pdTRUE;
}

void MqttManager::processQueuedMessages() {
    const uint32_t MAX_MESSAGES_PER_CYCLE = 5;
    uint32_t processedCount = 0;  // Local counter instead of static
    MQTTMessage msg;
    
    while (processedCount < MAX_MESSAGES_PER_CYCLE && 
           xQueueReceive(messageQueue, &msg, 0) == pdTRUE) {
        DEBUG_LOG_MQTT("Processing message %lu from queue - Topic: %s", 
                 processedCount, msg.topic);
        handleMessage(msg.topic, (byte*)msg.payload, msg.payloadLength);
        processedCount++;
    }
    
    if (processedCount > 0) {
        DEBUG_LOG_MQTT("Processed %lu messages this cycle", processedCount);
    }
}

void MqttManager::handleMessage(const char* topic, const byte* payload, unsigned int length) {
    static const uint32_t JSON_PROCESSING_TIMEOUT = 1000; // 1 second timeout for JSON processing
    static const uint32_t STATE_UPDATE_TIMEOUT = 2000;    // 2 second timeout for state updates
    
    if (!topic || !payload || length == 0 || length >= Config::MQTT::Message::MAX_PAYLOAD_LENGTH) {
        DEBUG_LOG_MQTT("Invalid message parameters");
        return;
    }

    // Store the start time for timeout checking
    unsigned long startTime = millis();

    // Copy payload to ensure null termination - done outside critical section
    char* payloadCopy = (char*)malloc(length + 1);
    if (!payloadCopy) {
        DEBUG_LOG_MQTT("Failed to allocate memory for payload");
        return;
    }
    
    memcpy(payloadCopy, payload, length);
    payloadCopy[length] = '\0';
    
    DEBUG_LOG_MQTT("Message received - Topic: %s, Payload: %s", topic, payloadCopy);

    // Parse JSON outside of any critical section
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payloadCopy);
    
    // Free the payload copy as soon as we're done with it
    free(payloadCopy);
    payloadCopy = nullptr;
    
    if (error) {
        DEBUG_LOG_MQTT("JSON parsing failed: %s", error.c_str());
        return;
    }

    // Check for JSON processing timeout
    if (millis() - startTime > JSON_PROCESSING_TIMEOUT) {
        DEBUG_LOG_MQTT("JSON processing timeout");
        return;
    }

    // Determine the message type and required action outside the critical section
    MessageAction action = determineMessageAction(topic);
    if (action == MessageAction::INVALID) {
        DEBUG_LOG_MQTT("Invalid message action for topic: %s", topic);
        return;
    }

    bool needsUpdate = false;
    bool success = false;

    // Minimal critical section for state updates
    {
        MutexGuard guard(messageMutex);
        if (!guard.isLocked()) {
            DEBUG_LOG_MQTT("Failed to acquire mutex for message handling");
            return;
        }

        // Check for timeout before proceeding with state update
        if (millis() - startTime > STATE_UPDATE_TIMEOUT) {
            DEBUG_LOG_MQTT("State update timeout before processing");
            return;
        }

        // Process message based on determined action
        switch (action) {
            case MessageAction::MODE:
                success = handleModeMessage(doc);
                needsUpdate = success;
                break;

            case MessageAction::NIGHT_MODE:
                success = handleNightModeMessage(doc);
                needsUpdate = success;
                break;

            case MessageAction::RECOVERY:
                success = handleRecoveryMessage(doc);
                needsUpdate = success;
                break;

            case MessageAction::NIGHT_SETTINGS:
                success = handleNightSettingsMessage(doc);
                needsUpdate = success;
                break;

            default:
                DEBUG_LOG_MQTT("Unhandled message action");
                break;
        }
    } // End of critical section

    // Final timeout check
    if (millis() - startTime > STATE_UPDATE_TIMEOUT) {
        DEBUG_LOG_MQTT("Message handling exceeded timeout");
        // Consider rolling back changes if needed
        return;
    }

    // Publish status update outside of critical section if needed
    if (needsUpdate) {
        publishStatus();
        DEBUG_LOG_MQTT("Status published after state update");
    }

    if (!success) {
        DEBUG_LOG_MQTT("Message handling failed for topic: %s", topic);
    }
}


// Helper method to determine message action from topic
MqttManager::MessageAction MqttManager::determineMessageAction(const char* topic) {
    if (strcmp(topic, Config::MQTT::Topics::Control::MODE) == 0) {
        return MessageAction::MODE;
    }
    else if (strcmp(topic, Config::MQTT::Topics::Control::NIGHT_MODE) == 0) {
        return MessageAction::NIGHT_MODE;
    }
    else if (strcmp(topic, Config::MQTT::Topics::Control::RECOVERY) == 0) {
        return MessageAction::RECOVERY;
    }
    else if (strcmp(topic, Config::MQTT::Topics::Control::NIGHT_SETTINGS) == 0) {
        return MessageAction::NIGHT_SETTINGS;
    }
    return MessageAction::INVALID;
}

bool MqttManager::handleNightModeMessage(const JsonDocument& doc) {
    DEBUG_LOG_MQTT("Processing night mode message");

    if (!doc["enabled"].is<bool>()) {
        DEBUG_LOG_MQTT("Night mode message missing or invalid 'enabled' field");
        return false;
    }

    bool enabled = doc["enabled"];
    return fanController.setNightMode(enabled);
}

bool MqttManager::handleRecoveryMessage(const JsonDocument& doc) {
    DEBUG_LOG_MQTT("Processing recovery message");

    if (!doc["recover"].is<bool>()) {
        DEBUG_LOG_MQTT("Recovery message missing or invalid 'recover' field");
        return false;
    }

    bool shouldRecover = doc["recover"];
    return shouldRecover ? fanController.attemptRecovery() : false;
}


bool MqttManager::handleModeMessage(const JsonDocument& doc) {
    DEBUG_LOG_MQTT("Processing mode message");

    if (!doc["mode"].is<const char*>()) {
        DEBUG_LOG_MQTT("Mode message missing or invalid 'mode' field");
        return false;
    }

    const char* mode = doc["mode"];
    DEBUG_LOG_MQTT("Setting mode to: %s", mode);

    if (strcmp(mode, "auto") == 0) {
        return fanController.setControlMode(FanController::Mode::AUTO);
    }
    else if (strcmp(mode, "manual") == 0) {
        bool modeResult = fanController.setControlMode(FanController::Mode::MANUAL);
        
        // Set speed if provided in manual mode
        if (modeResult && doc["speed"].is<int>()) {
            int speed = doc["speed"];
            DEBUG_LOG_MQTT("Setting manual speed to: %d", speed);
            fanController.setSpeedDutyCycle(speed);
        }
        return modeResult;
    }
    return false;
}

bool MqttManager::handleNightSettingsMessage(const JsonDocument& doc) {
    DEBUG_LOG_MQTT("Processing night settings message");

    // Validate required fields
    if (!doc["start_hour"].is<int>() || 
        !doc["end_hour"].is<int>() || 
        !doc["max_speed"].is<int>()) {
        DEBUG_LOG_MQTT("Night settings message missing required fields");
        return false;
    }

    int startHour = doc["start_hour"];
    int endHour = doc["end_hour"];
    int maxSpeed = doc["max_speed"];

    // Validate ranges
    if (startHour < 0 || startHour > 23 || 
        endHour < 0 || endHour > 23 || 
        maxSpeed < 0 || maxSpeed > 100) {
        DEBUG_LOG_MQTT("Night settings values out of range");
        return false;
    }

    return fanController.setNightSettings(startHour, endHour, maxSpeed);
}

/*******************************************************************************
 * Status Publishing
 ******************************************************************************/

void MqttManager::publishString(const char* topic, const String& value) {
    MutexGuard guard(messageMutex);
    if (!guard.isLocked()) {
        DEBUG_LOG_MQTT("Failed to acquire mutex for publishing");
        return;
    }

    if (mqttClient.connected()) {
        bool published = mqttClient.publish(topic, value.c_str());
        DEBUG_LOG_MQTT("Published to %s: %s (success: %d)", topic, value.c_str(), published);
    } else {
        DEBUG_LOG_MQTT("Failed to publish - not connected");
    }
}

void MqttManager::publishStatus() {
    if (!mqttClient.connected()) {
        DEBUG_LOG_MQTT("Cannot publish status - not connected");
        return;
    }

    MutexGuard guard(messageMutex);
    if (!guard.isLocked()) {
        DEBUG_LOG_MQTT("Failed to acquire mutex for status publish");
        return;
    }

    // System status document
    JsonDocument systemDoc;
    {
        auto status = fanController.getStatus();
        systemDoc["state"] = status == FanController::Status::OK ? "on" : "off";
        systemDoc["speed"] = fanController.getCurrentSpeed();
        systemDoc["mode"] = fanController.getControlMode() == FanController::Mode::AUTO ? "auto" : "manual";
        systemDoc["temperature"] = tempSensor.getSmoothedTemp();
        
        // Add error information if applicable
        if (status != FanController::Status::OK) {
            systemDoc["error"] = getFanStatusString(status);
        }
    }

    // Night mode status document
    JsonDocument nightDoc;
    {
        nightDoc["enabled"] = fanController.isNightModeEnabled();
        nightDoc["active"] = fanController.isNightModeActive();
        nightDoc["start_hour"] = fanController.getNightStartHour();
        nightDoc["end_hour"] = fanController.getNightEndHour();
        nightDoc["max_speed"] = fanController.getNightMaxSpeed();
    }

    // Publish both status documents
    bool systemPublished = publishJson(Config::MQTT::Topics::Status::SYSTEM, systemDoc);
    bool nightPublished = publishJson(Config::MQTT::Topics::Status::NIGHT_MODE, nightDoc);

    DEBUG_LOG_MQTT("Status published - System: %s, Night Mode: %s",
              systemPublished ? "success" : "failed",
              nightPublished ? "success" : "failed");
}

bool MqttManager::publishJson(const char* topic, const JsonDocument& doc) {
    if (!mqttClient.connected()) {
        return false;
    }

    char buffer[512];  // Increased buffer size for safety
    size_t n = serializeJson(doc, buffer);
    
    bool success = mqttClient.publish(topic, buffer, true);  // Set retained flag
    DEBUG_LOG_MQTT("Published to %s (%s): %s", topic, success ? "success" : "failed", buffer);
    
    return success;
}

/*******************************************************************************
 * Task Implementation
 ******************************************************************************/

void MqttManager::mqttTask(void* parameters) {
    MqttManager* mqtt = static_cast<MqttManager*>(parameters);
    DEBUG_LOG_MQTT("MQTT Task started");
    
    while (true) {
        mqtt->taskManager.updateTaskRunTime("MQTT");
        mqtt->processUpdate();
        
        // Use shorter delay when messages are being processed
        if (uxQueueMessagesWaiting(mqtt->messageQueue) > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));  // Fast response when active
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));  // Longer delay when idle
        }
    }
}

/*******************************************************************************
 * Utility Methods
 ******************************************************************************/
bool testConnection(const IPAddress& host, uint16_t port, int timeout_ms = 5000) {
    DEBUG_LOG_MQTT("Testing connection to %s:%d", host.toString().c_str(), port);
    
    WiFiClient client;
    client.setTimeout(timeout_ms / 1000);
    
    unsigned long start = millis();
    bool connected = client.connect(host, port);
    unsigned long duration = millis() - start;
    
    DEBUG_LOG_MQTT("Connection test took %lu ms, result: %s", 
              duration, connected ? "success" : "failed");
              
    client.stop();
    return connected;
}

const char* MqttManager::getFanStatusString(FanController::Status status) {
    switch (status) {
        case FanController::Status::OK:
            return "ok";
        case FanController::Status::ERROR:
            return "general_error";
        case FanController::Status::SHUTOFF:
            return "fan_stalled";
        default:
            return "unknown_error";
    }
}

uint32_t MqttManager::getTotalTimeout() {
  // Get the base reconnect delay from configuration
  uint32_t baseDelay = Config::MQTT::RECONNECT_DELAY_MS;

  // Calculate total timeout based on the backoff strategy
  uint32_t totalTimeout = 0;
  for (int i = 0; i < Config::MQTT::MAX_RETRIES; i++) {
    // Cast to uint32_t and limit the shift value directly without min
    uint32_t shiftAmount = (i > 5) ? 4U : (uint32_t)i; 
    uint32_t backoffDelay = baseDelay * (1U << shiftAmount);
    totalTimeout += backoffDelay;
  }

  return totalTimeout;
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