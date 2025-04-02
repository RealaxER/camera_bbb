#include "liveStream.h"

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
