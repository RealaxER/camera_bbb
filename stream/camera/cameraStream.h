#ifndef CAMERA_STREAM
#define CAMERA_STREAM

#include "baseStream.h"
#include "recordStream.h"
#include "liveStream.h"

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

#endif