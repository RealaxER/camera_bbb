#pragma once 

#include <mosquitto.h>

class Mqtt_t {
private:
    struct mosquitto *mosq;
public: 
    Mqtt_t(const char * id);

    int set_callback(void (*on_message)(struct mosquitto *, void *, const struct mosquitto_message *));

    int setup(const char * host, int port, int keepalive);

    int connect();

    int disconnect();

    int publish(const char * topic, const unsigned char * payload, int payload_len);

    int subscribe(const char * topic, int qos);
};

void __test_mqtt(void);