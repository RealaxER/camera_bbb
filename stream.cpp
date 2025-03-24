extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
}

#include <iostream>
#include <string>
#include <glog/logging.h>
#include <thread>
#include <atomic>

#define CAMERA_DEVICE_FILE "/dev/video2"
#define CAMERA_INPUT_FORMAT "v4l2"
#define CAMERA_FORMAT "mjpeg"


#define LOG_TAG_INFO(TAG, MESSAGE)    LOG(INFO) << "[" << TAG << "] " << MESSAGE
#define LOG_TAG_WARNING(TAG, MESSAGE) LOG(WARNING) << "[" << TAG << "] " << MESSAGE
#define LOG_TAG_ERROR(TAG, MESSAGE)   LOG(ERROR) << "[" << TAG << "] " << MESSAGE

#define TRACE_BEGIN() LOG(INFO) << " >>> " <<  __FUNCTION__
#define TRACE_END() LOG(INFO) << " <<< " <<  __FUNCTION__

#define STRINGIFY(x) #x
#define TOSTRING(x)  STRINGIFY(x)

enum class Result {
    SUCCESS = 0,
    INVALID_ARGUMENT,
    INVALID_STATE,
    FILE_NOT_FOUND,
    NETWORK_ERROR,
    UNKNOWN_ERROR
};

#define CAMERA_ASSERT(COND) \
    if (!(COND)) { \
        LOG(ERROR) << "Assert fail: " << TOSTRING(COND); \
        return Result::INVALID_STATE; \
    }

#define BASE_ASSERT(COND) \
    if (!(COND)) { \
        LOG(ERROR) << "Assert fail: " << TOSTRING(COND); \
        return false; \
    }


typedef enum {
    CameraClosed,
    CameraOpened,
    CameraStarted,
    CameraConfigured,
    CameraStopping,
    CameraRunning,
} CameraState;


struct CameraInfo {
    std::string format;
    std::string input_format;
    uint16_t height;
    uint16_t width;
    uint8_t fps;
};

class BaseStream {
public:
    Result open();
    Result configure();
    Result close();
    bool isSupportVideo();
    bool isSupportAudio();

    BaseStream(const std::string& device_name, int width, int height, int fps) {  
        file_name = device_name; 
        info.width = width;
        info.height = height;
        info.fps = fps;
        info.input_format = CAMERA_INPUT_FORMAT;  
        info.format = CAMERA_FORMAT;            
    }

    std::string file_name;
    int video_stream_index;
    AVFormatContext* format_ctx = nullptr;
private:
    CameraState mState;
    AVDictionary* options = nullptr;
    struct CameraInfo info;
};

Result BaseStream::configure() {
    std::string video_size = std::to_string(info.width) + "x" + std::to_string(info.height);

    avdevice_register_all();  
    av_dict_set(&options, "format", info.format.c_str(), 0); 
    av_dict_set(&options, "video_size", video_size.c_str(), 0); 
    av_dict_set(&options, "framerate", std::to_string(info.fps).c_str(), 0);       
    AVInputFormat* input_format = av_find_input_format(info.input_format.c_str());
    if (!input_format) {
        LOG(ERROR) << "Failed to find input format 'v4l2'" << std::endl;
        return Result::INVALID_ARGUMENT;
    }

    mState = CameraConfigured;
    
    return Result::SUCCESS;
}


Result BaseStream::open() {
    CAMERA_ASSERT(mState != CameraOpened);

    if (avformat_open_input(&format_ctx, file_name.c_str(), nullptr, nullptr) < 0) {
        LOG(ERROR) << "Can't open input.";
        return Result::INVALID_ARGUMENT;
    }
    if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
        LOG(ERROR) << "Fail to find info stream.";
        return Result::INVALID_ARGUMENT;
    }
    
    for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            info.fps = (uint8_t)av_q2d(format_ctx->streams[i]->avg_frame_rate);
            video_stream_index = i;
            break;
        }
    }
    mState = CameraOpened;

    return Result::SUCCESS;
}

Result BaseStream::close() {
    CAMERA_ASSERT(mState != CameraClosed);
    if (format_ctx) {
        avformat_close_input(&format_ctx);  
        format_ctx = nullptr;      
    }
    mState = CameraClosed; 

    return Result::SUCCESS;
}

bool BaseStream::isSupportVideo(){ 
    BASE_ASSERT(mState != CameraClosed);

    for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            return true;
        }
    }
    return false;
}

bool BaseStream::isSupportAudio(){
    BASE_ASSERT(mState != CameraClosed);

    for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            return true;
        }
    }
    return false;
}


class LiveStream {
public:
    LiveStream(std::shared_ptr<BaseStream> base) 
        : baseStream(base), mRunning(false) {         
    }
    Result stop();
    Result start();

private:
    std::thread mThread;      
    std::atomic<bool> mRunning; 
    CameraState mState;
    std::shared_ptr<BaseStream> baseStream; 

    void liveThread();
};


Result LiveStream::start() {
    CAMERA_ASSERT(mState != CameraStarted);

    mRunning = true; 
    mThread = std::thread(&LiveStream::liveThread, this);

    LOG(INFO) << "Streaming started on a separate thread!" << std::endl;
    mState = CameraStarted;

    return Result::SUCCESS;
}

Result LiveStream::stop() {
    CAMERA_ASSERT(mState != CameraClosed);

    mRunning = false;  
    if (mThread.joinable()) {
        mThread.join();  
    }
    mState = CameraClosed;

    return Result::SUCCESS;
}

void LiveStream::liveThread() {
    AVPacket* packet = av_packet_alloc();
    while (mRunning) {
        if(av_read_frame(baseStream->format_ctx, packet) >= 0 && (mState == CameraStarted)) {
            if (packet->stream_index == baseStream->video_stream_index) {
                // todo
            }
            av_packet_unref(packet);
        }else {
            LOG(ERROR) << "Fail to live stream";
            av_packet_free(&packet);
            return;
        }

    }
}

void print_help() {
    std::cout << "Usage: ./log_config [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --log_dir=<path>       Set the directory to save log files (default: /tmp/log)\n";
    std::cout << "  --minloglevel=<0-3>    Set the minimum log level (0: INFO, 1: WARNING, 2: ERROR, 3: FATAL)\n";
    std::cout << "  --alsologtostderr      Log to stderr in addition to files\n";
    std::cout << "  --help                 Display this help message\n";
}


class RecordStream {
public:
    RecordStream(std::shared_ptr<BaseStream> base) 
        : baseStream(base), mRunning(false) {         
    }

    Result stop();
    Result start();
private:
    std::thread mThread;      
    std::atomic<bool> mRunning; 
    CameraState mState;
    std::shared_ptr<BaseStream> baseStream; 

    void recordThread();
};



class CameraStream {
public:
    CameraStream(const std::string& device_name, int width, int height, int fps) {
        std::shared_ptr<BaseStream> baseStreamPtr = std::make_shared<BaseStream>(device_name, width, height, fps);

        live = std::make_unique<LiveStream>(baseStreamPtr);
        record = std::make_unique<RecordStream>(baseStreamPtr);

        baseStream = baseStreamPtr;
    }

    Result open();
    Result close();

private:
    std::unique_ptr<LiveStream> live;  
    std::unique_ptr<RecordStream> record;
    std::shared_ptr<BaseStream> baseStream;  // BaseStream được quản lý bởi shared_ptr
    CameraState mState;
};


Result CameraStream::open() {
    Result result = baseStream->configure();
    if (result != Result::SUCCESS) {
        LOG_TAG_ERROR(baseStream->file_name, "Failed to configure the stream");
        return result;
    }

    result = baseStream->open();
    if (result != Result::SUCCESS) {
        LOG_TAG_ERROR(baseStream->file_name, "Failed to open the stream");
        return result;
    }
    LOG_TAG_INFO(baseStream->file_name, "open camera");
    mState = CameraOpened;
    return Result::SUCCESS;
}


Result CameraStream::close() {
    Result result = baseStream->close();
    if (result != Result::SUCCESS) {
        LOG_TAG_ERROR(baseStream->file_name, "Failed to close the stream");
        return result;
    }
    LOG_TAG_INFO(baseStream->file_name, "close camera");
    mState = CameraOpened;
    
    return Result::SUCCESS;
}

int main(int argc, char* argv[]) {
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

    /*config stream camera*/

    CameraStream camera(CAMERA_DEVICE_FILE, 1280, 720, 30);
    camera.open();

    google::ShutdownGoogleLogging();
    return 0;
}
