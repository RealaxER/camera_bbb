#include "cameraStream.h"
#include <iostream>
#include "mqtt.h"
#include "p2p.h"
#include "typedef.pb.h"

#define DEVICE_NAME "camera-stream"
#define BROKER "127.0.0.1"
#define PORT 1883

#define SUB "camera/live/0xffff"

void print_help() {
    std::cout << "Usage: ./log_config [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --log_dir=<path>       Set the directory to save log files (default: /tmp/log)\n";
    std::cout << "  --minloglevel=<0-3>    Set the minimum log level (0: INFO, 1: WARNING, 2: ERROR, 3: FATAL)\n";
    std::cout << "  --alsologtostderr      Log to stderr in addition to files\n";
    std::cout << "  --help                 Display this help message\n";
}

Mqtt_t transport(DEVICE_NAME);

void mqtt_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) {
    // std::lock_guard<std::mutex> lock(mtx);
    // LOG_INFO("<-- " << message->topic << " : " << message->payload);
    // std::vector<uint8_t> vec_data((const uint8_t *)message->payload, (const uint8_t *)message->payload + message->payloadlen);
    // eventQueue.push(Event(Event::MQTT_MESSAGE_RECEIVED, message->topic, vec_data));
    // cv.notify_one();
}


int main(int argc, char* argv[]) {
    __test_mqtt();

    transport.set_callback(mqtt_callback);
    transport.setup(BROKER, PORT, 45);
    transport.subscribe(SUB , 1);
    transport.connect();

    google::InitGoogleLogging(argv[0]);

    // /*config google log and dir log */
    // std::string log_dir = "/tmp/log";  
    int min_log_level = 0;           
    bool log_to_stderr = true;        

    // for (int i = 1; i < argc; ++i) {
    //     std::string arg = argv[i];
    //     if (arg.find("--log_dir=") == 0) {
    //         log_dir = arg.substr(10);
    //     } else if (arg.find("--minloglevel=") == 0) {
    //         min_log_level = std::stoi(arg.substr(14));
    //     } else if (arg == "--alsologtostderr") {
    //         log_to_stderr = true;
    //     } else if (arg == "--help") {
    //         print_help();
    //         return 0;
    //     }
    // }

    //FLAGS_log_dir = log_dir;
    FLAGS_minloglevel = min_log_level;
    FLAGS_alsologtostderr = log_to_stderr;
    FLAGS_colorlogtostderr = 1;  
    //av_log_set_level(AV_LOG_DEBUG);

    /*config stream camera*/
    CameraStream camera(CAMERA_DEVICE_FILE, 640, 360, 30);
    camera.configure();
    camera.open();
    camera.setSupportRecord(true);
    camera.start(ChaseMode);

    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(1)); 
        LOG(INFO) << "Sleep 1s";
    }

    google::ShutdownGoogleLogging();
    return 0;
}
