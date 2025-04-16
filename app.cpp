#include "cameraStream.h"
#include <iostream>
#include "mqtt.h"
#include "p2p.h"
#include "typedef.pb.h"
#include <vector>
#include <regex>

#define MAX_BUFFER 1024

#define MAX_MESSAGE 1048576

#define DEVICE_NAME "app-stream"
#define BROKER "test.mosquitto.org"
#define PORT 1883

#define SUB "server/live/+"

Mqtt_t mqtt(DEVICE_NAME);

P2P p2p;
std::string mac_device; 

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
    p2p.pushEvent(event_des);

    std::vector<std::string> candidate_strings;

    for (int i = 0; i < p2p_proto.candidate_size(); ++i) {
        candidate_strings.push_back(p2p_proto.candidate(i));
        LOG(INFO) << "Candidate[" << i << "]: " << p2p_proto.candidate(i);
    }

    std::string best_candidate_type = select_best_candidate(candidate_strings);
    LOG(INFO) << "Best candidate type: " << best_candidate_type;
    
    Event event_candidate = {Event::Type::SetLocalCandidate, std::string(best_candidate_type)};
    p2p.pushEvent(event_candidate);
}

void mqtt_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) {
    LOG(INFO) << "<-- " << message->topic << " : Received " << message->payloadlen << " bytes";

    Transport_t receivedTransport;

    if (!receivedTransport.ParseFromArray(message->payload, message->payloadlen)) {
        LOG(ERROR) << "Failed to parse protobuf payload!";
        return;
    }

    LOG(INFO) << "Received MAC: " << receivedTransport.mac();
    mac_device = receivedTransport.mac();
    if (receivedTransport.p2p_size() > 0) {
        const ProtoP2p_t& p2p = receivedTransport.p2p(0);  // Get the first P2P object
        process_received_p2p(p2p);
    }
}

int main(int argc, char* argv[]) {

    google::InitGoogleLogging(argv[0]);

    int min_log_level = 0;           
    bool log_to_stderr = true;        
    FLAGS_minloglevel = min_log_level;
    FLAGS_alsologtostderr = log_to_stderr;
    FLAGS_colorlogtostderr = 1;  

    p2p.SetStunServer("stun.l.google.com:19302");
    p2p.SetMaxMessageSize(MAX_MESSAGE);
    p2p.CreatePeerConnection();
    p2p.HandleIncomingDataChannel();

    mqtt.set_callback(mqtt_callback);
    mqtt.setup(BROKER, PORT, 45);
    mqtt.subscribe(SUB , 1);
    mqtt.connect();

    Event event;

    LOG(INFO) << "=====APP STREAM START======";


    Transport_t transport;
	transport.set_mac("");  
	ProtoP2p_t* p2p_info = transport.add_p2p();

    while(1){
        if (p2p.popEvent(event)) {
            switch (event.type) {
                case Event::Type::SetLocalDescription:
                    LOG(INFO) << "=====Processing Set Local Description=====: " << event.data;
                    p2p.setRemoteDescription(event.data);
                    break;
                case Event::Type::SetLocalCandidate:
                    LOG(INFO) << "=====Processing Set Local Candidate=======: " << event.data;
                    p2p.addRemoteCandidate(event.data);                
                    break;
                
                case Event::Type::LocalDescription:
                    LOG(INFO) << "====Processing Local Description====: " << event.data;
                    p2p_info->set_description(event.data);
                    break;
                case Event::Type::LocalCandidate:
                    LOG(INFO) << "Processing Local Candidate: " << event.data;
                    p2p_info->add_candidate(event.data);                
                    break;

                case Event::Type::StateChange:
                    LOG(INFO) << "Processing State Change: " << event.data;
                    if (event.data.find("Connected") != std::string::npos) { 
                        auto datachannel = p2p.GetDataChannelByIndex(0);
                        if (datachannel != nullptr) {
                            LOG(INFO) << "First DataChannel label: " << datachannel->label();
                            p2p.SendMessageReturnChannel(datachannel, "BUI DINH HIEN SERVER");
                        }
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
                        std::string topic = "camera/live/" + mac_device;

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

    return 0;
}