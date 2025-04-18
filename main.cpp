#include "cameraStream.h"
#include <iostream>
#include "mqtt.h"
#include "p2p.h"
#include "typedef.pb.h"
#include <ifaddrs.h>
#include <netpacket/packet.h> 
#include <vector>
#include <regex>

#define MAX_BUFFER 1024


#define MAX_MESSAGE 1048576

#define DEVICE_NAME "camera-stream"
#define BROKER "broker.emqx.io"
#define PORT 1883

#define SUB "camera/live/"

#define CAMERA_DEVICE_FILE "/dev/video2"

void print_help() {
    std::cout << "Usage: ./log_config [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --log_dir=<path>       Set the directory to save log files (default: /tmp/log)\n";
    std::cout << "  --minloglevel=<0-3>    Set the minimum log level (0: INFO, 1: WARNING, 2: ERROR, 3: FATAL)\n";
    std::cout << "  --alsologtostderr      Log to stderr in addition to files\n";
    std::cout << "  --help                 Display this help message\n";
}


std::string get_mac_address(const std::string& interface) {
    struct ifaddrs *ifap, *ifa;
    struct sockaddr_ll *s;
    std::string mac_addr = "";

    if (getifaddrs(&ifap) == -1) {
        std::cerr << "Error getting interfaces" << std::endl;
        return mac_addr;
    }

    for (ifa = ifap; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr->sa_family == AF_PACKET) { 
            s = (struct sockaddr_ll*) ifa->ifa_addr;
            if (ifa->ifa_name == interface) {
                char mac[18];
                snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                        s->sll_addr[0], s->sll_addr[1], s->sll_addr[2],
                        s->sll_addr[3], s->sll_addr[4], s->sll_addr[5]);
                mac_addr = mac;
                break;
            }
        }
    }

    freeifaddrs(ifap);
    return mac_addr;
}


Mqtt_t mqtt(DEVICE_NAME);

std::shared_ptr<P2P> p2p = std::make_shared<P2P>();  

std::string parse_candidate_type(const std::string& candidate_str) {
    std::regex regex(R"(a=candidate:\d+ \d+ \w+ \d+ [\d\.a-f:]+ \d+ typ (\w+))");
    std::smatch match;

    if (std::regex_search(candidate_str, match, regex)) {
        return match[1].str(); 
    }

    return ""; 
}

std::string select_best_candidate(const std::vector<std::string>& candidate_strings) {
    std::string best_candidate;

    bool found_host = false;
    for (const auto& candidate_str : candidate_strings) {
        std::string type = parse_candidate_type(candidate_str);

        if (type == "host" && !found_host) {
            best_candidate = candidate_str;  
            found_host = true;
        } else if (type == "srflx" && !found_host) {
            best_candidate = candidate_str;  
        }
    }

    return best_candidate;
}

void process_received_p2p(const ProtoP2p_t& p2p_proto) {
    auto description = p2p_proto.description();
    LOG(INFO) << "Description: " << description;
    Event event_des = {Event::Type::SetLocalDescription, description};
    p2p->pushEvent(event_des);

    std::vector<std::string> candidate_strings;

    for (int i = 0; i < p2p_proto.candidate_size(); ++i) {
        candidate_strings.push_back(p2p_proto.candidate(i));
        LOG(INFO) << "Candidate[" << i << "]: " << p2p_proto.candidate(i);
    }

    std::string best_candidate_type = select_best_candidate(candidate_strings);
    LOG(INFO) << "Best candidate type: " << best_candidate_type;
    
    Event event_candidate = {Event::Type::SetLocalCandidate, std::string(best_candidate_type)};
    p2p->pushEvent(event_candidate);
}

void mqtt_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) {
    LOG(INFO) << "<-- " << message->topic << " : " << (const char*)message->payload;
    std::string payload = (const char*)message->payload;

    Transport_t receivedTransport;

    if (!receivedTransport.ParseFromArray(message->payload, message->payloadlen)) {
        LOG(ERROR) << "Failed to parse protobuf payload!";
        return;
    }

    LOG(INFO) << "Received MAC: " << receivedTransport.mac();
    
    //compare mac here
    if (receivedTransport.p2p_size() > 0) {
        const ProtoP2p_t& p2p = receivedTransport.p2p(0);  // Get the first P2P object
        process_received_p2p(p2p);
    }
}


int main(int argc, char* argv[]) {
    //__test_mqtt();

    //std::string mac = get_mac_address("wlp3s0");

    std::string mac = "ec:2e:98:e3:d6:a5";

    p2p->SetStunServer("stun.l.google.com:19302");
    p2p->SetMaxMessageSize(MAX_MESSAGE);
    p2p->CreatePeerConnection();
    std::string label = "camera/live";
    p2p->CreateDataChannel(label);

    mqtt.set_callback(mqtt_callback);
    mqtt.setup(BROKER, PORT, 45);
    std::string topic_sub = SUB + mac;
    mqtt.subscribe(topic_sub.c_str() , 1);
    mqtt.connect();

    google::InitGoogleLogging(argv[0]);

    int min_log_level = 0;           
    bool log_to_stderr = true;        


    //FLAGS_log_dir = log_dir;
    FLAGS_minloglevel = min_log_level;
    FLAGS_alsologtostderr = log_to_stderr;
    FLAGS_colorlogtostderr = 1;  
    //av_log_set_level(AV_LOG_DEBUG);

    /*config stream camera*/
    CameraStream camera(CAMERA_DEVICE_FILE, 640, 480, 30);
    camera.configure();
    camera.open();
    //camera.setSupportRecord(true);
    camera.start(LiveMode);

	Transport_t transport;
	transport.set_mac(mac);  
	ProtoP2p_t* p2p_info = transport.add_p2p();
    Event event;

    while (true) {
        if (p2p->popEvent(event)) {
            switch (event.type) {
                case Event::Type::SetLocalDescription:
                    LOG(INFO) << "=====Processing Set Local Description=====: " << event.data;
                    p2p->setRemoteDescription(event.data);
                    break;
                case Event::Type::SetLocalCandidate:
                    LOG(INFO) << "=====Processing Set Local Candidate=======: " << event.data;
                    p2p->addRemoteCandidate(event.data);                
                    break;
                
                case Event::Type::LocalDescription:
                    LOG(INFO) << "Processing Local Description: " << event.data;
                    p2p_info->set_description(event.data);
                    break;
                    
                case Event::Type::LocalCandidate:
                    LOG(INFO) << "Processing Local Candidate: " << event.data;
                    p2p_info->add_candidate(event.data);                
                    break;

                case Event::Type::StateChange:
                    LOG(INFO) << "Processing State Change: " << event.data;
                    if (event.data.find("Connected") != std::string::npos) { 
                        p2p->sendMessageToChannel(label, "BUI DINH HIEN");
                    
                        LOG(INFO) << "[REMOTE max message size:"  << p2p->pc->remoteMaxMessageSize() << "]";
                        camera.streamLive(p2p, label);
                        //camera.streamRecord(p2p, label);
                    }
                    break;

                case Event::Type::GatheringStateChange:
                    LOG(INFO) << "Processing Gathering State Change: " << event.data;
                    if (event.data.find("Complete") != std::string::npos) { 
                        LOG(INFO) << "ICE Gathering is complete!";

                        uint8_t serialized_data[MAX_BUFFER];
                        memset(serialized_data , 0, sizeof(serialized_data));    

                        transport.SerializeToArray(&serialized_data, sizeof(serialized_data));
                        LOG(INFO) << "Byte: " << transport.ByteSizeLong();
                        std::string topic = "server/live/" + mac;

                        int ret = mqtt.publish(topic.c_str(), serialized_data, transport.ByteSizeLong());
                        if (ret != MOSQ_ERR_SUCCESS) {
                            LOG(ERROR) << "Failed to send message: " << mosquitto_strerror(ret) << std::endl;
                        }
                        p2p_info->Clear(); 
                    }
                    break;
            }
        }else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
        }
    }


    google::ShutdownGoogleLogging();
    return 0;
}
