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
    Result stream(std::shared_ptr<P2P> p2p, std::string label);

    
private:
    std::thread mThread; 
    std::thread mStream;      
    std::atomic<bool> mRunning; 
    CameraState mState;
    std::shared_ptr<BaseStream> baseStream; 
    std::shared_ptr<P2P> transport;
    std::atomic<bool> mP2P;
    std::string mLabel;

    void recordThread();

    AVCodec* decoder = nullptr;
    AVFormatContext* record_format_ctx = nullptr;
    struct SwsContext* sws_ctx = nullptr;
    AVCodecContext* decoder_ctx = nullptr;
    AVCodecContext* encoder_ctx = nullptr;
    AVStream* video_stream = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* yuv_frame = nullptr;
    AVCodec* encoder = nullptr;
    size_t currentPosition = 0;
};



#endif