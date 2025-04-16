#include "recordStream.h"
#include <fstream>

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
    std::vector<uint8_t> fileData;
    const size_t chunkSize = 614400;

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

                        // if (mP2P) {
                        //     if (!transport->streamBuffereToChannel(mLabel, packet->data, packet->size)) {
                        //         LOG(ERROR) << "Failed to send file data over DataChannel";
                        //     } else {
                        //         LOG(INFO) << "[Sent " << packet->size << " bytes of file data to server]";
                        //     }
                        // }
                        av_packet_unref(packet);
                    }
                }
            }
            av_packet_unref(packet);
        }
    }
}

Result RecordStream::stream(std::shared_ptr<P2P> p2p, std::string label) {
    CAMERA_ASSERT(mState != CameraClosed);
    
    transport = p2p;
    mLabel = label;
    mP2P = true;

    return Result::SUCCESS;
}