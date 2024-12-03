#ifndef SECRETS_H
#define SECRETS_H

namespace Secrets {
    namespace WiFi {
        constexpr const char SSID[] = "your_wifi_ssd_here";
        constexpr const char PASSWORD[] = "your_wifi_password_here";
    }

    namespace MQTT {
        constexpr const char USERNAME[] = "your_mqtt_username_here";
        constexpr const char PASSWORD[] = "your_mqtt_password_here";
        constexpr const char SERVER[] = "your_mqtt_server_here";
        constexpr const char CLIENT_ID[] = "your_mqtt_client_id_here";
        constexpr const char BASE_TOPIC[] = "fan_controller_esp_32";
        constexpr uint16_t PORT = 1883;
    }
}

#endif // SECRETS_H