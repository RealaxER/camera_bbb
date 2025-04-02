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

private:
    std::thread mThread;      
    std::atomic<bool> mRunning; 
    CameraState mState;
    std::shared_ptr<BaseStream> baseStream; 

    void liveThread();
};



#endif