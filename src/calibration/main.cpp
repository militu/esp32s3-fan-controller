#include <Arduino.h>
#include "config.h"

volatile uint32_t pulseCount = 0;
void IRAM_ATTR handleTachInterrupt();

class FanCalibration {
private:
    const uint8_t PWM_PIN = Config::Fan::PWM::PWM_PIN;
    const uint8_t TACH_PIN = Config::Fan::PWM::TACH_PIN;
    const uint32_t PWM_FREQ = Config::Fan::PWM::FREQUENCY;
    const uint8_t PWM_RESOLUTION = Config::Fan::PWM::RESOLUTION;
    const uint8_t PWM_CHANNEL = Config::Fan::PWM::CHANNEL;
    const uint8_t PULSES_PER_REV = Config::Fan::RPM::PULSES_PER_REV;
    static const uint32_t MEASURE_INTERVAL = 2000;

    void printDivider() {
        Serial.print("\r\n");  // Ensure clean line start
        for (int i = 0; i < 60; i++) {
            Serial.print("-");
        }
        Serial.print("\r\n");
    }

    void printHeader(const char* text) {
        Serial.print("\r\n");  // Clean line before header
        for (int i = 0; i < 60; i++) {
            Serial.print("=");
        }
        Serial.print("\r\n");
        Serial.print(text);
        Serial.print("\r\n");
        for (int i = 0; i < 60; i++) {
            Serial.print("=");
        }
        Serial.print("\r\n");
    }

    uint32_t calculateRPM() {
        uint32_t pulses = pulseCount;
        pulseCount = 0;
        return (pulses * 60) / (MEASURE_INTERVAL / 1000.0) / PULSES_PER_REV;
    }

    uint8_t percentToRawPWM(uint8_t percent) {
        return map(percent, 0, 100, 
                  Config::Fan::Speed::MIN_PWM,
                  Config::Fan::Speed::MAX_PWM);
    }

public:
    void begin() {
        printHeader("Fan Calibration Initialization");
        
        ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
        ledcAttachPin(PWM_PIN, PWM_CHANNEL);
        
        // Setup tachometer
        pinMode(TACH_PIN, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(TACH_PIN), handleTachInterrupt, RISING);
        
        // Start with fan off
        ledcWrite(PWM_CHANNEL, 0);
        delay(1000);
        
        Serial.print("Initialization complete\r\n");
    }
    
    void runCalibration() {
        printHeader("Fan Calibration Starting");
        
        // Print configuration
        Serial.print("Configuration:");
        printDivider();
        
        char buffer[80];  // Buffer for formatting strings
        
        snprintf(buffer, sizeof(buffer), "PWM Frequency            : %d Hz\r\n", PWM_FREQ);
        Serial.print(buffer);
        snprintf(buffer, sizeof(buffer), "PWM Resolution           : %d bits\r\n", PWM_RESOLUTION);
        Serial.print(buffer);
        snprintf(buffer, sizeof(buffer), "Min PWM                  : %d\r\n", Config::Fan::Speed::MIN_PWM);
        Serial.print(buffer);
        snprintf(buffer, sizeof(buffer), "Max PWM                  : %d\r\n", Config::Fan::Speed::MAX_PWM);
        Serial.print(buffer);
        
        Serial.print("\r\nMeasurements:");
        printDivider();
        
        // Table header with fixed widths
        Serial.print("Speed     Raw PWM   RPM       Effective Speed\r\n");
        printDivider();

        // Run measurements
        for (int percent = 0; percent <= 100; percent += 5) {
            uint8_t rawPWM = percentToRawPWM(percent);
            ledcWrite(PWM_CHANNEL, rawPWM);
            delay(2000);
            pulseCount = 0;
            delay(MEASURE_INTERVAL);
            uint32_t rpm = calculateRPM();
            float effectivePercent = rpm > 0 ? (float)rpm / Config::Fan::RPM::MAXIMUM * 100.0 : 0;

            // Format each line using snprintf for consistent spacing
            snprintf(buffer, sizeof(buffer), "%-9d %-9d %-9d %-9.1f\r\n", 
                    percent, rawPWM, rpm, effectivePercent);
            Serial.print(buffer);
        }

        ledcWrite(PWM_CHANNEL, 0);
        printHeader("Calibration Complete");
    }

    void runSingleTest(uint8_t percent) {
        printHeader("Single Test Result");
        
        uint8_t rawPWM = percentToRawPWM(percent);
        ledcWrite(PWM_CHANNEL, rawPWM);
        delay(2000);
        pulseCount = 0;
        delay(MEASURE_INTERVAL);
        uint32_t rpm = calculateRPM();
        float effectivePercent = (float)rpm / Config::Fan::RPM::MAXIMUM * 100.0;

        char buffer[80];
        snprintf(buffer, sizeof(buffer), "Requested Speed          : %d%%\r\n", percent);
        Serial.print(buffer);
        snprintf(buffer, sizeof(buffer), "PWM Value               : %d\r\n", rawPWM);
        Serial.print(buffer);
        snprintf(buffer, sizeof(buffer), "RPM                     : %d\r\n", rpm);
        Serial.print(buffer);
        snprintf(buffer, sizeof(buffer), "Effective Speed         : %.1f%%\r\n", effectivePercent);
        Serial.print(buffer);
    }

    void printHelp() {
        printHeader("Available Commands");
        Serial.print("c    : Run full calibration\r\n");
        Serial.print("tXX  : Test specific percentage (e.g., t50 for 50%)\r\n");
        Serial.print("h    : Show this help\r\n");
    }
};

// Global instance
FanCalibration calibration;

void IRAM_ATTR handleTachInterrupt() {
    pulseCount++;
}

void setup() {
    pinMode(Config::Hardware::PIN_POWER_ON, OUTPUT);
    digitalWrite(Config::Hardware::PIN_POWER_ON, HIGH);

    Serial.begin(115200);
    
    // Wait for serial port to connect for ESP32-S3
    unsigned long startTime = millis();
    while (!Serial && (millis() - startTime) < 5000) {
        delay(10);
    }
    
    calibration.begin();
    calibration.printHelp();
}

void loop() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        
        if (cmd == "c") {
            calibration.runCalibration();
        }
        else if (cmd == "h") {
            calibration.printHelp();
        }
        else if (cmd.startsWith("t")) {
            int percent = cmd.substring(1).toInt();
            if (percent >= 0 && percent <= 100) {
                calibration.runSingleTest(percent);
            } else {
                Serial.print("Invalid percentage. Use 0-100\r\n");
            }
        }
        else {
            Serial.print("Unknown command. Type 'h' for help\r\n");
        }
    }
    delay(100);
}