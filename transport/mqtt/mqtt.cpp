#include <iostream>
#include <string>
#include <mqtt.h>
#include <unistd.h>

#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define LOG_INFO(...) std::cout << ANSI_COLOR_GREEN << "[INFO] " << ANSI_COLOR_RESET << __VA_ARGS__ << std::endl
#define LOG_ERR(...) std::cout << ANSI_COLOR_RED << "[ERR] " << ANSI_COLOR_RESET << __VA_ARGS__ << std::endl


Mqtt_t::Mqtt_t(const char * id) {
    mosquitto_lib_init();
    this->mosq = mosquitto_new(id, true, NULL);
}

int Mqtt_t::set_callback(void (*on_message)(struct mosquitto *, void *, const struct mosquitto_message *)) {
    if(this->mosq != nullptr) {
        mosquitto_message_callback_set(this->mosq, on_message);
        return 0;
    }
    return -1;
}

int Mqtt_t::setup(const char * host, int port, int keepalive) {
    if(this->mosq != nullptr) {
        mosquitto_connect_async(this->mosq, host, port, keepalive);      
        return 0;
    }
    return -1;
}

int Mqtt_t::connect() {
    if(this->mosq != nullptr) {
        mosquitto_loop_start(this->mosq);
    }
    return -1;
}

int Mqtt_t::disconnect() {
    if(this->mosq != nullptr) {
        mosquitto_disconnect(this->mosq); // Ngắt kết nối
        mosquitto_destroy(this->mosq); // Hủy bỏ client

        mosquitto_lib_cleanup(); // Dọn dẹp thư viện Mosquitto
        return 0;
    }
    return -1;
}

int Mqtt_t::publish(const char * topic, const unsigned char * payload, int payload_len) {
    if(this->mosq != nullptr) {
        mosquitto_publish(this->mosq, NULL, topic, payload_len, payload, 1, false);
        LOG_INFO("--> " << topic << " : " << payload);
        return 1;
    }
    return -1;
}

int Mqtt_t::subscribe(const char * topic, int qos) {
    if(this->mosq != nullptr){
        return mosquitto_subscribe(this->mosq, NULL, topic, qos);
    }
    return -1;
}


void __test_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) {
    if(message->payloadlen) {
        std::cout << ANSI_COLOR_GREEN << "PASS" << ANSI_COLOR_RESET << " : mqtt sub" << std::endl;
    } else {
        std::cout << ANSI_COLOR_RED << "PASS" << ANSI_COLOR_RESET << " : mqtt sub" << std::endl;
    }
}

void __test_mqtt(void) {
    class Mqtt_t mqtt_client("__test_mqtt");
    mqtt_client.set_callback(__test_message_callback);
    mqtt_client.setup("127.0.0.1", 1883, 60);
    mqtt_client.subscribe("test_mqtt", 1);
    mqtt_client.connect();
    std::string payload = "hello world";
    const unsigned char * payload_u8 = (const unsigned char *)payload.c_str();
    if(mqtt_client.publish("test_mqtt", payload_u8, payload.size())) {
        std::cout << ANSI_COLOR_GREEN << "PASS" << ANSI_COLOR_RESET << " : mqtt publish" << std::endl;
    }
    sleep(1);
    mqtt_client.disconnect();
}