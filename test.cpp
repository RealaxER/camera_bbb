extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavutil/log.h>  
}

#include <iostream>
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

Result BaseStream::configure() {
    std::string video_size = std::to_string(info.width) + "x" + std::to_string(info.height);

    avdevice_register_all();  
    av_dict_set(&options, "format", info.format.c_str(), 0); 
    av_dict_set(&options, "video_size", video_size.c_str(), 0); 
    av_dict_set(&options, "framerate", std::to_string(info.fps).c_str(), 0); 
    input_format = av_find_input_format(info.input_format.c_str());
    if (!input_format) {
        LOG(ERROR) << "Failed to find input format 'v4l2'";
        return Result::INVALID_ARGUMENT;
    }

    mState = CameraConfigured;
    
    return Result::SUCCESS;
}


Result BaseStream::open() {
    CAMERA_ASSERT(mState != CameraOpened);

    if (avformat_open_input(&format_ctx, file_name.c_str(), input_format, &options) < 0) {
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
        }
        if(format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_index = i;
        }
    }

    if (video_stream_index == -1) {
        LOG(INFO) << "Fail to find video stream.";
        return Result::UNKNOWN_ERROR;
    }
    mState = CameraOpened;

    return Result::SUCCESS;
}

uint8_t BaseStream::getFps(){
    return info.fps;
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

    LOG(INFO) << "Streaming started on a separate thread!";
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
        if (av_read_frame(baseStream->format_ctx, packet) >= 0 && (mState == CameraStarted)) {
            if (packet->stream_index == baseStream->video_stream_index) {
                baseStream->packetQueue.push(packet);
            }
        } else {
            LOG(ERROR) << "Fail to live stream";
            av_packet_free(&packet); 
            return; 
        }
    }

    av_packet_free(&packet);  
}

class RecordStream {
public:
    RecordStream(std::shared_ptr<BaseStream> base) 
        : baseStream(base), mRunning(false) {         
    }

    Result stop();
    Result start();
    Result open();
    Result close();
    
private:
    std::thread mThread;      
    std::atomic<bool> mRunning; 
    CameraState mState;
    std::shared_ptr<BaseStream> baseStream; 

    void recordThread();


    AVCodec* decoder = nullptr;
    struct SwsContext* sws_ctx = nullptr;
    AVFormatContext* record_format_ctx = nullptr;
    AVCodecContext* decoder_ctx = nullptr;
    AVCodecContext* encoder_ctx = nullptr;
    AVStream* video_stream = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* yuv_frame = nullptr;
    AVCodec* encoder = nullptr;
};

Result RecordStream::open() {
    uint8_t fps = baseStream->getFps();

    decoder = avcodec_find_decoder(baseStream->format_ctx->streams[baseStream->video_stream_index]->codecpar->codec_id);
    if (!decoder) {
        LOG(ERROR) << "Failed to find decoder for input.";
        return Result::INVALID_ARGUMENT; 
    }

    // Create decoder context and copy parameters from the input stream
    decoder_ctx = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(decoder_ctx, baseStream->format_ctx->streams[baseStream->video_stream_index]->codecpar);
    if (avcodec_open2(decoder_ctx, decoder, nullptr) < 0) {
        LOG(ERROR) << "Failed to open codec decoder.";
        return Result::INVALID_ARGUMENT;
    }

    avformat_alloc_output_context2(&record_format_ctx, nullptr, CAMERA_RECORD_FORMAT, CAMERA_RECORD_FILE);
    if (!record_format_ctx) {
        LOG(ERROR) << "Failed to create output format context.";
        return Result::INVALID_ARGUMENT;
    }

    // Find the encoder (H.264) and allocate encoder context
    encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!encoder) {
        LOG(ERROR) << "Failed to find H.264 encoder.";
        return Result::INVALID_ARGUMENT;
    }

    encoder_ctx = avcodec_alloc_context3(encoder);

    // Lower resolution and bitrate for lightweight encoding
    encoder_ctx->bit_rate = 500000; // Lower bitrate: 500 kbps (360p)
    encoder_ctx->width = decoder_ctx->width;       // Downscale to 640x360 (360p)
    encoder_ctx->height = decoder_ctx->height;
    encoder_ctx->time_base = AVRational{1, fps}; 
    encoder_ctx->framerate = AVRational{fps, 1};
    encoder_ctx->gop_size = 25;      
    encoder_ctx->max_b_frames = 2;   
    encoder_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    // Set encoder options for fast encoding
    av_opt_set(encoder_ctx->priv_data, "preset", "ultrafast", 0); // Fastest encoding preset
    av_opt_set(encoder_ctx->priv_data, "tune", "zerolatency", 0); // Low latency
    av_opt_set(encoder_ctx->priv_data, "flags", "+cgop", 0);      // Closed GOP

    LOG(INFO) << "Encoder resolution: " << encoder_ctx->width << "x" << encoder_ctx->height;

    if (avcodec_open2(encoder_ctx, encoder, nullptr) < 0) {
        LOG(ERROR) << "Failed to open H.264 encoder.";
        return Result::INVALID_ARGUMENT;
    }

    video_stream = avformat_new_stream(record_format_ctx, nullptr);
    avcodec_parameters_from_context(video_stream->codecpar, encoder_ctx);
    video_stream->time_base = encoder_ctx->time_base;

    if (avio_open(&record_format_ctx->pb, CAMERA_RECORD_FILE, AVIO_FLAG_WRITE) < 0) {
        LOG(ERROR) << "Failed to open output file.";
        return Result::INVALID_ARGUMENT;
    }
    if (avformat_write_header(record_format_ctx, nullptr) < 0) {
        LOG(ERROR) << "Failed to write header for output file.";
        return Result::INVALID_ARGUMENT;
    }

    if (decoder_ctx->pix_fmt == AV_PIX_FMT_YUVJ420P ||
        decoder_ctx->pix_fmt == AV_PIX_FMT_YUVJ422P ||
        decoder_ctx->pix_fmt == AV_PIX_FMT_YUVJ444P) {
        LOG(WARNING) << "Converting JPEG-based pixel format to YUV420P.";
        decoder_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    }

    sws_ctx = sws_getContext(
        decoder_ctx->width, decoder_ctx->height, decoder_ctx->pix_fmt,
        encoder_ctx->width, encoder_ctx->height, encoder_ctx->pix_fmt,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    frame = av_frame_alloc();
    yuv_frame = av_frame_alloc();
    yuv_frame->format = encoder_ctx->pix_fmt;
    yuv_frame->width = encoder_ctx->width;
    yuv_frame->height = encoder_ctx->height;
    av_image_alloc(yuv_frame->data, yuv_frame->linesize, yuv_frame->width, yuv_frame->height, encoder_ctx->pix_fmt, 32);

    mState = CameraOpened;
    return Result::SUCCESS;
}

Result RecordStream::start(){
    CAMERA_ASSERT(mState != CameraStarted);

    mRunning = true; 
    mThread = std::thread(&RecordStream::recordThread, this);

    LOG(INFO) << "Record started on a separate thread!";
    mState = CameraStarted;

    return Result::SUCCESS;
}

Result RecordStream::stop() {
    CAMERA_ASSERT(mState != CameraClosed);

    mRunning = false; 
    if (mThread.joinable()) {
        mThread.join();  
    }
    mState = CameraStopping;

    return Result::SUCCESS;
}

Result RecordStream::close(){
    CAMERA_ASSERT(mState != CameraClosed);

    if (record_format_ctx) {
        av_write_trailer(record_format_ctx);
        if (record_format_ctx->pb) {
            avio_closep(&record_format_ctx->pb);
        }
        avformat_free_context(record_format_ctx);
        record_format_ctx = nullptr;
    }

    if (encoder_ctx) {
        avcodec_free_context(&encoder_ctx);
        encoder_ctx = nullptr;
    }
    
    if (decoder_ctx) {
        avcodec_free_context(&decoder_ctx);
        decoder_ctx = nullptr;
    }

    if (yuv_frame) {
        av_freep(&yuv_frame->data[0]);
        av_frame_free(&yuv_frame);
    }

    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = nullptr;
    }

    mState = CameraClosed;

    return Result::SUCCESS;
}

void RecordStream::recordThread() {
    int64_t last_pts = AV_NOPTS_VALUE;  // Track last PTS to ensure monotonic increase

    while (mRunning) {
        AVPacket* packet = baseStream->packetQueue.pop();  
        if (packet) {
            LOG(INFO) << "Recording packet (size: " << packet->size << ")";
            if(packet->size > 0){
                avcodec_send_packet(decoder_ctx, packet);
                while (avcodec_receive_frame(decoder_ctx, frame) >= 0) {
                    sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, yuv_frame->data, yuv_frame->linesize);

                    yuv_frame->pts = av_rescale_q(frame->pts, baseStream->format_ctx->streams[baseStream->video_stream_index]->time_base, encoder_ctx->time_base);
                    if (yuv_frame->pts != AV_NOPTS_VALUE && yuv_frame->pts <= last_pts) {
                        yuv_frame->pts = last_pts + 1;  
                    }
                    last_pts = yuv_frame->pts;

                    avcodec_send_frame(encoder_ctx, yuv_frame);
                    while (avcodec_receive_packet(encoder_ctx, packet) >= 0) {
                        packet->pts = av_rescale_q(packet->pts, encoder_ctx->time_base, video_stream->time_base);
                        packet->dts = av_rescale_q(packet->dts, encoder_ctx->time_base, video_stream->time_base);

                        if (packet->pts < packet->dts) {
                            packet->pts = packet->dts;
                        }

                        av_interleaved_write_frame(record_format_ctx, packet);
                        av_packet_unref(packet);
                    }
                }
            }
            av_packet_unref(packet);
        }
    }
}

class CameraStream {
public:
    CameraStream(const std::string& device_name, int width, int height, int fps) {
        std::shared_ptr<BaseStream> baseStreamPtr = std::make_shared<BaseStream>(device_name, width, height, fps);

        live = std::make_unique<LiveStream>(baseStreamPtr);
        record = std::make_unique<RecordStream>(baseStreamPtr);

        baseStream = baseStreamPtr;
    }
    
    Result configure();
    Result open();
    Result close();
    Result start(CameraStreamMode mode);

    bool doesSupportRecord(){
        return mSupportRecord;
    }
    void setSupportRecord(bool status){
        mSupportRecord = status;
    }
private:
    bool mSupportRecord = false;
    std::unique_ptr<LiveStream> live;  
    std::unique_ptr<RecordStream> record;
    std::shared_ptr<BaseStream> baseStream;  // BaseStream được quản lý bởi shared_ptr
    CameraState mState;
};


Result CameraStream::configure() {
    Result result = baseStream->configure();
    if (result != Result::SUCCESS) {
        LOG_TAG_ERROR(baseStream->file_name, "Failed to configure the stream");
        return result;
    }

    LOG_TAG_INFO(baseStream->file_name, "Configure camera");
    mState = CameraConfigured;
    return Result::SUCCESS;
}

Result CameraStream::open() {
    CAMERA_ASSERT(mState == CameraConfigured);

    Result result = baseStream->open();
    if (result != Result::SUCCESS) {
        LOG_TAG_ERROR(baseStream->file_name, "Failed to open the stream");
        return result;
    }

    result = record->open();
    if (result != Result::SUCCESS) {
        LOG_TAG_ERROR(baseStream->file_name, "Failed to open record");
        return result;
    }

    LOG_TAG_INFO(baseStream->file_name, "Open camera");
    mState = CameraOpened;
    return Result::SUCCESS;
}

Result CameraStream::start(CameraStreamMode mode) {
    CAMERA_ASSERT(mState != CameraConfigured || mState != CameraOpened);

    switch (mode)
    {
        case LiveMode:
            live->start();
            break;

        case RecordMode:
            if(doesSupportRecord()){
                record->start();
            }
            break;
        case ChaseMode:
            live->start();
            if(doesSupportRecord()){
                record->start();
            }
            break;
    }

    mState = CameraStarted;
    return Result::SUCCESS;
}

Result CameraStream::close() {
    CAMERA_ASSERT(mState != CameraClosed);

    Result result = baseStream->close();
    if (result != Result::SUCCESS) {
        LOG_TAG_ERROR(baseStream->file_name, "Failed to close the stream");
        return result;
    }
    LOG_TAG_INFO(baseStream->file_name, "close camera");
    mState = CameraClosed;
    
    return Result::SUCCESS;
}


void print_help() {
    std::cout << "Usage: ./log_config [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --log_dir=<path>       Set the directory to save log files (default: /tmp/log)\n";
    std::cout << "  --minloglevel=<0-3>    Set the minimum log level (0: INFO, 1: WARNING, 2: ERROR, 3: FATAL)\n";
    std::cout << "  --alsologtostderr      Log to stderr in addition to files\n";
    std::cout << "  --help                 Display this help message\n";
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
