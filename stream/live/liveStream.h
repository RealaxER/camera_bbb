#ifndef LIVE_STREAM
#define LIVE_STREAM


#include "baseStream.h"

class LiveStream {
public:
    LiveStream(std::shared_ptr<BaseStream> base) 
        : baseStream(base), mRunning(false) {         
    }
    Result stop();
    Result start();
    Result stream(std::shared_ptr<P2P> p2p, std::string label);

private:
    std::thread mThread;      
    std::atomic<bool> mRunning; 
    CameraState mState;
    std::shared_ptr<BaseStream> baseStream; 
    std::shared_ptr<P2P> transport;
    std::atomic<bool> mP2P = false;
    std::string mLabel;


    struct SwsContext* sws_ctx = nullptr;
    AVCodecContext* decoder_ctx = nullptr;
    AVCodecContext* encoder_ctx = nullptr;


    AVFrame* frame = nullptr;
    AVFrame* yuv_frame = nullptr;

    void liveThread();
};



#endif