#ifndef BASE_STREAM
#define BASE_STREAM

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavutil/log.h>  
}

#include <string>
#include <glog/logging.h>
#include <thread>
#include <atomic>
#include <sstream>
#include <iomanip> 

#include <queue>
#include <mutex>
#include <condition_variable>


#define CAMERA_DEVICE_FILE "/dev/video2"
#define CAMERA_INPUT_FORMAT "v4l2"
#define CAMERA_RECORD_FORMAT "mpegts"
#define CAMERA_LIVE_FORMAT "mjpeg"
#define CAMERA_RECORD_FILE "output.ts"

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

typedef enum {
    LiveMode,
    RecordMode,
    ChaseMode,
}CameraStreamMode;

struct CameraInfo {
    std::string format;
    std::string input_format;
    uint16_t height;
    uint16_t width;
    uint8_t fps;
};


// Thread-safe queue
class PacketQueue {
public:
    void push(AVPacket* packet) {
        std::lock_guard<std::mutex> lock(mMutex);
        mQueue.push(packet);
        mCondVar.notify_one();  // Notify record thread when packet is available
    }

    AVPacket* pop() {
        std::unique_lock<std::mutex> lock(mMutex);
        mCondVar.wait(lock, [this]() { return !mQueue.empty(); });  // Wait if queue is empty
        AVPacket* packet = mQueue.front();
        mQueue.pop();
        return packet;
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(mMutex);
        return mQueue.empty();
    }

private:
    std::queue<AVPacket*> mQueue;
    std::mutex mMutex;
    std::condition_variable mCondVar;
};

class BaseStream {
public:
    Result open();
    Result configure();
    Result close();
    bool isSupportVideo();
    bool isSupportAudio();
    uint8_t getFps();

    BaseStream(const std::string& device_name, int width, int height, int fps) {  
        file_name = device_name; 
        info.width = width;
        info.height = height;
        info.fps = fps;
        info.input_format = CAMERA_INPUT_FORMAT;  
        info.format = CAMERA_LIVE_FORMAT;            
    }

    std::string file_name;
    int video_stream_index;
    int audio_stream_index;
    AVFormatContext* format_ctx = nullptr;
    PacketQueue packetQueue;
private:
    CameraState mState;
    AVDictionary* options = nullptr;
    struct CameraInfo info;
    AVInputFormat* input_format;
};



#endif