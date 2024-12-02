# ESP32-S3 Fan Controller

A fan controller system built with ESP32-S3 (originally for the Lilygo T-Display-S3), featuring temperature-based control, MQTT integration, persistent configuration, and a TFT display interface. (including a wokwi simulation) 

[![ESP32-S3 Fan Controller Wokwi Screenshot](./assets/wokwi-screenshot.png?)](./assets/wokwi-screenshot.png?)

## Key Features

- **Temperature Control**

  - Automatic fan speed adjustment based on temperature readings
  - DS18B20 temperature sensor integration
  - Configurable temperature thresholds and response curves
  - Temperature smoothing for stable operation

- **Fan Management**

  - PWM-based speed control with RPM monitoring
  - Automatic and manual operation modes
  - Stall detection and automatic recovery
  - Configurable minimum and maximum speed limits
  - RPM feedback monitoring
  - Persistent configuration of operating mode and settings

- **Night Mode**

  - Configurable quiet hours operation
  - Automatic time-based activation with NTP sync
  - Adjustable maximum speed during night hours
  - Manual override capability
  - Persistent night mode settings between restarts

- **Configuration Persistence**

  - Fan operation mode (Auto/Manual)
  - Manual speed settings
  - Night mode state and settings
  - Settings preserved across power cycles
  - ESP32's Non-Volatile Storage (NVS) based

- **Display Interface**

  - Real-time temperature and fan speed visualization
  - Status indicators for WiFi, MQTT, and night mode
  - Boot screen with initialization progress
  - Customizable dashboard layout
  - Support for both ILI9341 and LilyGO S3 displays

- **Network Connectivity**
  - WiFi connection with automatic reconnection
  - MQTT integration for remote monitoring and control
  - NTP synchronization for accurate timekeeping

## Hardware Support

### Compatible Displays

- ILI9341 TFT Display (320x240)
- LilyGO S3 Display (320x170)

### Required Components

- ESP32-S3 DevKitC-1 or compatible board
- DS18B20 Temperature Sensor
- 4-wire PWM Fan with tachometer
- Power supply appropriate for fan
- Pull-up resistors for temperature sensor and fan tachometer

## Software Dependencies

- **Framework**

  - Arduino Framework for ESP32
  - FreeRTOS

- **Core Libraries**

  - LVGL (v8.x) for UI
  - OneWire & DallasTemperature for sensor
  - PubSubClient for MQTT
  - ArduinoJson for data handling

- **Display Drivers**
  - Adafruit GFX Library
  - Adafruit ILI9341
  - ESP LCD (for LilyGO S3)

## Configuration

The system is highly configurable through the `config.h` file. Here are some key examples of available settings (see `config.h` for complete configuration options):

### Example Temperature Settings

```cpp
// See config.h for complete temperature sensor configuration
#define TEMP_MIN_TEMP    25.0  // Minimum temperature for fan activation
#define TEMP_MAX_TEMP    45.0  // Temperature for maximum fan speed
#define TEMP_SMOOTH_SAMPLES 10  // Number of samples for temperature smoothing
```

### Example Fan Settings

```cpp
// See config.h for complete fan configuration
#define FAN_MIN_SPEED    10    // Minimum fan speed (%)
#define FAN_MAX_SPEED    100   // Maximum fan speed (%)
#define FAN_MIN_PWM      26    // Minimum PWM value
#define FAN_MAX_PWM      255   // Maximum PWM value
```

### Example Night Mode Settings

```cpp
// See config.h for complete night mode configuration
#define NIGHT_MODE_START    22  // Night mode start hour (24h format)
#define NIGHT_MODE_END      7   // Night mode end hour
#define NIGHT_MODE_MAX_SPEED 40 // Maximum speed during night mode (%)
```

For a complete list of configuration options, including debug settings, WiFi parameters, MQTT configuration, and display settings, please refer to the `config.h` file in the project root.

### MQTT Topics

#### Status Topics

- `fan_controller/status` - General system status
- `fan_controller/temperature` - Current temperature readings
- `fan_controller/available` - System availability

#### Control Topics

- `fan_controller/mode` - Fan mode control
- `fan_controller/night_mode` - Night mode control
- `fan_controller/night_settings` - Night mode configuration

## Project Structure

```
├── src/
│   ├── display/
│   │   ├── boot_screen.*      # Boot/initialization display
│   │   ├── dashboard_screen.* # Main monitoring interface
│   │   ├── display_driver.*   # Hardware abstraction
│   │   └── display_manager.*  # Display state management
│   ├── core/
│   │   ├── fan_controller.*   # Fan control logic
│   │   ├── temp_sensor.*      # Temperature monitoring
│   │   ├── task_manager.*     # FreeRTOS management
│   │   └── config_preference.* # Persistent configuration
│   ├── network/
│   │   ├── mqtt_manager.*     # MQTT communication
│   │   ├── wifi_manager.*     # WiFi connectivity
│   │   └── ntp_manager.*      # Time synchronization
│   └── config.h              # System configuration
```

## Configuration Persistence

The system maintains the following settings across power cycles:

```cpp
// Persistent Fan Settings
- Fan Operation Mode (Auto/Manual)
- Manual Speed Setting
- Night Mode State (Enabled/Disabled)
- Night Mode Start Hour
- Night Mode End Hour
- Night Mode Maximum Speed
```

These settings are automatically saved when changed and restored on system startup.

## Building and Installation

1. Clone the repository
2. Install PlatformIO
3. Configure settings in `config.h`
4. Build and upload:
   ```bash
   pio run -t upload
   ```

## Home Assistant Integration

### MQTT Configuration

Add the following configuration to your Home Assistant's MQTT configuration (e.g., `configuration.yaml` or through the MQTT integration):

```yaml
mqtt:
  sensor:
    - name: "ESP32 Fan Temperature"
      state_topic: "fan_controller_topic/status"
      value_template: "{{ value_json.temp }}"
      unit_of_measurement: "°C"
      availability_topic: "fan_controller_topic/available"
      payload_available: "online"
      payload_not_available: "offline"
      expire_after: 30

    - name: "ESP32 Fan RPM"
      state_topic: "fan_controller_topic/status"
      value_template: "{{ value_json.rpm }}"
      unit_of_measurement: "RPM"
      availability_topic: "fan_controller_topic/available"
      payload_available: "online"
      payload_not_available: "offline"
      expire_after: 30

    - name: "ESP32 Fan Current Speed"
      state_topic: "fan_controller_topic/status"
      value_template: "{{ value_json.fan_speed }}"
      unit_of_measurement: "%"
      availability_topic: "fan_controller_topic/available"
      payload_available: "online"
      payload_not_available: "offline"
      expire_after: 30

  switch:
    - name: "ESP32 Fan Night Mode"
      state_topic: "fan_controller_topic/status"
      command_topic: "fan_controller_topic/night_mode"
      value_template: "{{ value_json.night_mode_enabled }}"
      payload_on: '{"enabled":true}'
      payload_off: '{"enabled":false}'
      state_on: true
      state_off: false
      icon: "mdi:weather-night"
      availability_topic: "fan_controller_topic/available"

  number:
    - name: "ESP32 Fan Manual Speed"
      state_topic: "fan_controller_topic/status"
      value_template: "{{ value_json.fan_speed }}"
      command_topic: "fan_controller_topic/mode"
      command_template: '{"mode": "manual", "speed": {{ value }} }'
      min: 0
      max: 100
      step: 1
      icon: "mdi:fan"

    - name: "ESP32 Fan Night Start Hour"
      state_topic: "fan_controller_topic/status"
      value_template: "{{ value_json.night_start }}"
      command_topic: "fan_controller_topic/night_settings"
      command_template: >
        {"start_hour": {{ value }}, "end_hour": {{ states('number.esp32_fan_night_end_hour') }}, "max_speed": {{ states('number.esp32_fan_night_max_speed') }}}
      min: 0
      max: 23
      step: 1

    - name: "ESP32 Fan Night End Hour"
      state_topic: "fan_controller_topic/status"
      value_template: "{{ value_json.night_end }}"
      command_topic: "fan_controller_topic/night_settings"
      command_template: >
        {"start_hour": {{ states('number.esp32_fan_night_start_hour') }}, "end_hour": {{ value }}, "max_speed": {{ states('number.esp32_fan_night_max_speed') }}}
      min: 0
      max: 23
      step: 1

    - name: "ESP32 Fan Night Max Speed"
      state_topic: "fan_controller_topic/status"
      value_template: "{{ value_json.night_max_speed }}"
      command_topic: "fan_controller_topic/night_settings"
      command_template: >
        {"start_hour": {{ states('number.esp32_fan_night_start_hour') }}, "end_hour": {{ states('number.esp32_fan_night_end_hour') }}, "max_speed": {{ value }}}
      min: 0
      max: 100
      step: 1

  select:
    - name: "ESP32 Fan Mode"
      state_topic: "fan_controller_topic/status"
      command_topic: "fan_controller_topic/mode"
      value_template: "{{ value_json.mode }}"
      options:
        - "auto"
        - "manual"
      command_template: '{"mode": "{{ value }}"}'

  button:
    - name: "ESP32 Fan Recovery"
      command_topic: "fan_controller_topic/recovery"
      payload_press: '{"recover": true}'
```

### Dashboard Card Configuration

Create a custom dashboard card using this YAML configuration:

```yaml
type: vertical-stack
cards:
  - type: vertical-stack
    cards:
      - type: custom:mini-graph-card
        entities:
          - entity: sensor.esp32_fan_temperature
            name: Temperature
          - entity: sensor.esp32_fan_rpm
            name: RPM
            y_axis: secondary
        hours_to_show: 24
        points_per_hour: 4
        line_width: 2
        hour24: true
        show:
          labels: true
          points: false
          legend: true
      - type: entities
        title: Fan Control
        show_header_toggle: false
        entities:
          - entity: select.esp32_fan_mode
            name: Fan Mode
          - type: conditional
            conditions:
              - entity: select.esp32_fan_mode
                state: manual
            row:
              entity: number.esp32_fan_manual_speed
              name: Fan Speed
              icon: mdi:fan
          - type: custom:mushroom-entity-card
            entity: switch.esp32_fan_night_mode
            name: Night Mode
            icon: mdi:weather-night
            tap_action:
              action: toggle
            fill_container: false
          - entity: number.esp32_fan_night_start_hour
            name: Night Start
            icon: mdi:clock-start
          - entity: number.esp32_fan_night_end_hour
            name: Night End
            icon: mdi:clock-end
          - entity: number.esp32_fan_night_max_speed
            name: Night Max Speed
            icon: mdi:speedometer
          - type: button
            name: Recovery
            icon: mdi:restart
            tap_action:
              action: call-service
              service: button.press
              target:
                entity_id: button.esp32_fan_recovery
```

Requirements:

- MQTT integration must be configured in Home Assistant
- Custom cards required:
  - [mini-graph-card](https://github.com/kalkih/mini-graph-card)
  - [mushroom-cards](https://github.com/piitaya/lovelace-mushroom)

## License

This project is licensed under the MIT License. See LICENSE file for details.
