// =============================================================================
// MQTT Manager Implementation
// Handles MQTT communication, message processing, and status updates for a fan 
// control system with temperature monitoring
// =============================================================================

#define DEBUG_LOG(msg, ...) if (DEBUG_MQTT) { Serial.printf(msg "\n", ##__VA_ARGS__); }
#include "mqtt_manager.h"

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * Tests TCP connection to MQTT broker
 * @param host Broker IP address
 * @param port Broker port
 * @param timeout_ms Connection timeout in milliseconds
 * @return true if connection successful
 */
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

// =============================================================================
// Constructor & Initialization
// =============================================================================

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
    // Initialize synchronization primitives
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

// =============================================================================
// Connection Management
// =============================================================================

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

    // Configure MQTT client
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
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

void MqttManager::connect() {
    DEBUG_LOG("Entering connect method");
    
    DEBUG_LOG("About to acquire connection mutex");
    MutexGuard guard(connectionMutex);
    if (!guard.isLocked()) {
        DEBUG_LOG("Failed to acquire connection mutex in connect()");
        return;
    }
    DEBUG_LOG("Connection mutex acquired successfully");

    // Generate unique client ID
    String clientId = MQTT_CLIENT_ID;
    clientId += "_";
    clientId += String(random(0xffff), HEX);
    DEBUG_LOG("Created client ID: %s", clientId.c_str());

    // Resolve broker address
    IPAddress broker;
    if (!WiFi.hostByName(MQTT_SERVER, broker)) {
        DEBUG_LOG("Failed to resolve MQTT broker address");
        return;
    }
    DEBUG_LOG("MQTT broker IP resolved to: %s", broker.toString().c_str());

    // Test TCP connection
    if (!testConnection(broker, MQTT_PORT)) {
        DEBUG_LOG("Failed to establish TCP connection to broker");
        return;
    }
    DEBUG_LOG("TCP connection test successful");

    // Attempt connection with retries
    int retries = 0;
    bool connected = false;

    while (!connected && retries < MQTT_MAX_RETRIES) {
        DEBUG_LOG("Starting connection attempt %d/%d to %s", retries + 1, MQTT_MAX_RETRIES, broker.toString().c_str());
        
        unsigned long startTime = millis();
        
        // Connect with will message
        connected = mqttClient.connect(clientId.c_str(), 
                                     nullptr,    // username
                                     nullptr,    // password
                                     MQTT_AVAILABILITY_TOPIC,  // will topic
                                     1,          // will QoS
                                     true,       // will retain
                                     "offline");  // will message
        
        unsigned long endTime = millis();
        DEBUG_LOG("Connection attempt took %lu ms", endTime - startTime);

        if (connected) {
            DEBUG_LOG("MQTT Connected Successfully!");
            // Publish initial online status
            mqttClient.publish(MQTT_AVAILABILITY_TOPIC, "online", true);

            if (setupSubscriptions()) {
                DEBUG_LOG("Topics Subscribed Successfully");
                wasConnected = true;
                updateAvailable = true;
                
                // Send initial status after successful connection
                publishStatus();
                break;
            } else {
                DEBUG_LOG("Failed to subscribe to topics!");
                connected = false;
            }
        } else {
            int state = mqttClient.state();
            DEBUG_LOG("Connection failed! State: %s (%d)", 
                     getMQTTStateString(state).c_str(), state);
            retries++;
            if (retries < MQTT_MAX_RETRIES) {
                DEBUG_LOG("Waiting %d ms before next attempt", MQTT_RECONNECT_DELAY);
                vTaskDelay(pdMS_TO_TICKS(MQTT_RECONNECT_DELAY));
            }
        }
    }
}

// =============================================================================
// Message Processing
// =============================================================================

void MqttManager::handleMessage(const char* topic, const byte* payload, unsigned int length) {
    if (!topic || !payload || length == 0 || length >= MAX_PAYLOAD_LENGTH) {
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

        if (strcmp(topic, MQTT_FAN_COMMAND_TOPIC) == 0) {
            handleModeMessage(doc);
        }
        else if (strcmp(topic, MQTT_NIGHT_MODE_COMMAND_TOPIC) == 0) {
            handleNightModeMessage(doc);
        }
        else if (strcmp(topic, MQTT_RECOVERY_TOPIC) == 0) {
            handleRecoveryMessage(doc);
        }
        else if (strcmp(topic, MQTT_NIGHT_SETTINGS_COMMAND_TOPIC) == 0) {
            handleNightSettingsMessage(doc);
        }
    }

    free(payloadCopy);
    publishStatus();
}

// Message handlers for specific topics
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
        
        // Set PWM if provided
        if (modeResult && doc["pwm"].is<int>()) {
            int pwm = doc["pwm"];
            DEBUG_LOG("Setting manual PWM to: %d", pwm);
            bool pwmResult = fanController.setPWMDutyCycle(pwm);
            DEBUG_LOG("Set PWM result: %s", pwmResult ? "success" : "failed");
        }
    }
}

void MqttManager::handleNightSettingsMessage(const JsonDocument& doc) {
    DEBUG_LOG("Processing night settings message: %s", doc.as<String>().c_str());

    // Validate required fields
    if (!doc["start_hour"].is<int>() || 
        !doc["end_hour"].is<int>() || 
        !doc["max_pwm"].is<int>()) {
        DEBUG_LOG("Night settings message missing or invalid required fields");
        return;
    }

    // Get and validate values
    int startHour = doc["start_hour"];
    int endHour = doc["end_hour"];
    int maxPWM = doc["max_pwm"];

    if (startHour < 0 || startHour > 23 || 
        endHour < 0 || endHour > 23 || 
        maxPWM < 0 || maxPWM > 100) {
        DEBUG_LOG("Night settings values out of range");
        return;
    }

    // Convert PWM percentage to raw value
    uint8_t rawMaxPWM = FanController::convertPercentToPWM(
        maxPWM, 
        fanController.getConfig().minPWM, 
        fanController.getConfig().maxPWM
    );

    // Update settings
    bool result = fanController.setNightSettings(startHour, endHour, rawMaxPWM);
    DEBUG_LOG("Night settings update result: %s", result ? "success" : "failed");
}

// =============================================================================
// Status Publishing
// =============================================================================

void MqttManager::publishStatus() {
    MutexGuard guard(messageMutex);
    if (!guard.isLocked()) {
        DEBUG_LOG("Failed to acquire mutex for status publish");
        return;
    }

    JsonDocument doc;
    
    // Populate status document
    doc["status"] = fanController.getStatus() == FanController::Status::OK ? "ok" : "error";
    doc["fan_pwm"] = fanController.getCurrentPWM();
    doc["target_pwm"] = fanController.getTargetPWM();
    doc["rpm"] = fanController.getMeasuredRPM();
    doc["mode"] = fanController.getControlMode() == FanController::Mode::AUTO ? "auto" : "manual";
    doc["temp"] = tempSensor.getSmoothedTemp();
    doc["night_mode"] = fanController.isNightModeActive();
    doc["night_start"] = fanController.getNightStartHour();
    doc["night_end"] = fanController.getNightEndHour();
    doc["night_max_pwm"] = FanController::convertPWMToPercent(
        fanController.getNightMaxPWM(),
        fanController.getConfig().minPWM,
        fanController.getConfig().maxPWM
    );

    // Serialize and publish
    char buffer[512];
    size_t n = serializeJson(doc, buffer);
    
    if (mqttClient.connected()) {
        bool success = mqttClient.publish(MQTT_FAN_STATE_TOPIC, buffer, true);
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

// =============================================================================
// Task Management & Updates
// =============================================================================

void MqttManager::processUpdate() {
    if (!initialized) {
        DEBUG_LOG("MQTT not initialized");
        return;
    }

    if (!WiFi.isConnected()) {
        DEBUG_LOG("WiFi not connected");
        return;
    }

    // Handle periodic client processing
    unsigned long now = millis();
    if (now - lastClientLoop >= 10) {  // Every 10ms
        lastClientLoop = now;
        mqttClient.loop();
    }

    // Publish availability status
    if (mqttClient.connected() && now - lastAvailabilityPublish >= AVAILABILITY_INTERVAL) {
        lastAvailabilityPublish = now;
        mqttClient.publish(MQTT_AVAILABILITY_TOPIC, "online", true);
        DEBUG_LOG("Published availability status");
    }

    // Process message queue
    processQueuedMessages();

    // Update status periodically
    if (now - lastStatusUpdate >= MQTT_UPDATE_INTERVAL) {
        lastStatusUpdate = now;
        if (mqttClient.connected()) {
            publishStatus();
        }
    }

    // Handle reconnection if needed
    bool needsReconnect = false;
    {
        MutexGuard guard(connectionMutex);
        if (!guard.isLocked()) return;

        bool currentlyConnected = mqttClient.connected();
        
        if (!currentlyConnected) {
            if (now - lastConnectAttempt >= MQTT_RECONNECT_DELAY) {
                needsReconnect = true;
                lastConnectAttempt = now;
            }
        }
    }

    if (needsReconnect) {
        connect();
    }
}

void MqttManager::mqttTask(void* parameters) {
    MqttManager* mqtt = static_cast<MqttManager*>(parameters);
    DEBUG_LOG("MQTT Task started");
    
    while (true) {
        mqtt->taskManager.updateTaskRunTime("MQTT");
        mqtt->processUpdate();
        vTaskDelay(pdMS_TO_TICKS(50));  // Run every 50ms
    }
}

// =============================================================================
// Message Queue Management
// =============================================================================

void MqttManager::processQueuedMessages() {
    static uint32_t messageCount = 0;
    MQTTMessage msg;
    
    while (xQueueReceive(messageQueue, &msg, 0) == pdTRUE) {
        messageCount++;
        DEBUG_LOG("Processing message %lu from queue - Topic: %s", messageCount, msg.topic);
        handleMessage(msg.topic, (byte*)msg.payload, msg.payloadLength);
    }
}

bool MqttManager::enqueueMessage(const char* topic, const byte* payload, unsigned int length) {
    if (!messageQueue) {
        DEBUG_LOG("Message queue not initialized");
        return false;
    }

    // Prepare message for queue
    MQTTMessage msg;
    strncpy(msg.topic, topic, MAX_TOPIC_LENGTH - 1);
    msg.topic[MAX_TOPIC_LENGTH - 1] = '\0';
    
    size_t copyLength = min(length, (unsigned int)(MAX_PAYLOAD_LENGTH - 1));
    memcpy(msg.payload, payload, copyLength);
    msg.payload[copyLength] = '\0';
    msg.payloadLength = copyLength;

    // Add to queue with timeout
    BaseType_t result = xQueueSend(messageQueue, &msg, pdMS_TO_TICKS(QUEUE_TIMEOUT_MS));
    DEBUG_LOG("Message enqueued: %s", result == pdTRUE ? "success" : "failed");
    return result == pdTRUE;
}

// =============================================================================
// Utility Methods
// =============================================================================

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

bool MqttManager::setupSubscriptions() {
    MutexGuard guard(messageMutex);
    if (!guard.isLocked()) {
        DEBUG_LOG("Failed to acquire mutex for subscriptions");
        return false;
    }

    // Subscribe to all required topics
    bool success = true;
    success &= mqttClient.subscribe(MQTT_FAN_COMMAND_TOPIC);
    success &= mqttClient.subscribe(MQTT_FAN_PRESET_COMMAND_TOPIC);
    success &= mqttClient.subscribe(MQTT_NIGHT_MODE_COMMAND_TOPIC);
    success &= mqttClient.subscribe(MQTT_NIGHT_SETTINGS_COMMAND_TOPIC);
    success &= mqttClient.subscribe(MQTT_RECOVERY_TOPIC);
    
    DEBUG_LOG("Subscriptions setup %s", success ? "successful" : "failed");
    return success;
}

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