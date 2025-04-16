#include "cameraStream.h"

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

Result CameraStream::streamLive(std::shared_ptr<P2P> p2p, std::string label) {
    CAMERA_ASSERT(mState != CameraClosed);
    return live->stream(p2p, label);
}

Result CameraStream::streamRecord(std::shared_ptr<P2P> p2p, std::string label) {
    CAMERA_ASSERT(mState != CameraClosed);
    return record->stream(p2p, label);
}