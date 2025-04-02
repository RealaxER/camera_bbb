#include "baseStream.h"


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