#ifndef RECORD_STREAM
#define RECORD_STREAM

#include "baseStream.h"

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



#endif