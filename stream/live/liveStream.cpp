#include "liveStream.h"
#include <cstring>
#include <iomanip>
#include <stdio.h>


#define av_err2str_cpp(errnum) \
    ({ char error_string[AV_ERROR_MAX_STRING_SIZE] = {0}; \
        av_make_error_string(error_string, AV_ERROR_MAX_STRING_SIZE, errnum); \
        error_string; })


#define av_ts2str_cpp(ts) \
({ char ts_string[AV_TS_MAX_STRING_SIZE] = {0}; \
    av_ts_make_string(ts_string, ts); \
    ts_string; })

#define av_ts2timestr_cpp(ts, tb) \
    ({ char ts_time_string[AV_TS_MAX_STRING_SIZE] = {0}; \
        av_ts_make_time_string(ts_time_string, ts, tb); \
        ts_time_string; })

static void log_packet_tb(AVRational *time_base_ctx, const AVPacket *pkt)
{
    AVRational *time_base = time_base_ctx;

    printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
            av_ts2str_cpp(pkt->pts), av_ts2timestr_cpp(pkt->pts, time_base),
            av_ts2str_cpp(pkt->dts), av_ts2timestr_cpp(pkt->dts, time_base),
            av_ts2str_cpp(pkt->duration), av_ts2timestr_cpp(pkt->duration, time_base),
            pkt->stream_index);
}  


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

void parseSPSandPPS(uint8_t *extradata, size_t extradata_size) {
    // H.264 NAL Unit Start Code: 0x00 0x00 0x00 0x01
    const uint8_t start_code[] = {0x00, 0x00, 0x00, 0x01};

    // Loop through the extradata and find NAL units
    size_t pos = 0;
    while (pos + 4 <= extradata_size) {
        // Search for NAL start code (0x00 0x00 0x00 0x01)
        if (extradata[pos] == start_code[0] && extradata[pos+1] == start_code[1] &&
            extradata[pos+2] == start_code[2] && extradata[pos+3] == start_code[3]) {
            // We found a start code, now extract the NAL unit
            size_t nal_start = pos + 4;
            size_t nal_end = nal_start;

            // Find the next NAL start code or end of extradata
            while (nal_end + 4 <= extradata_size && !(extradata[nal_end] == start_code[0] &&
                extradata[nal_end+1] == start_code[1] &&
                extradata[nal_end+2] == start_code[2] &&
                extradata[nal_end+3] == start_code[3])) {
                nal_end++;
            }

            // Extract the NAL unit
            size_t nal_size = nal_end - nal_start;
            if (nal_size > 0) {
                uint8_t nal_unit[nal_size];
                std::memcpy(nal_unit, &extradata[nal_start], nal_size);

                // Check if it's SPS or PPS by the first byte
                if (nal_unit[0] == 0x67) {
                    // SPS found
                    LOG(INFO) << "SPS found, size: " << nal_size << " bytes";
                    // Print all bytes of the SPS
                    LOG(INFO) << "SPS data: ";
                    for (size_t i = 0; i < nal_size; ++i) {
                        LOG(INFO) << "0x" << std::hex << (int)nal_unit[i] << " ";
                    }
                    LOG(INFO) << std::endl;
                    // Process the SPS data here (nal_unit contains the SPS)
                } else if (nal_unit[0] == 0x68) {
                    // PPS found
                    LOG(INFO) << "PPS found, size: " << nal_size << " bytes";
                    // Print all bytes of the PPS
                    LOG(INFO) << "PPS data: ";
                    for (size_t i = 0; i < nal_size; ++i) {
                        LOG(INFO) << "0x" << std::hex << (int)nal_unit[i] << " ";
                    }
                    LOG(INFO) << std::endl;
                    // Process the PPS data here (nal_unit contains the PPS)
                }
            }
            pos = nal_end; // Move past the current NAL unit
        } else {
            pos++;
        }
    }
}

void showExtradata(uint8_t *extradata, size_t extradata_size) {
    LOG(INFO) << "Extradata bytes: \n";
    
    // Lặp qua từng byte và in ra dưới dạng hexa
    for (size_t i = 0; i < extradata_size; ++i) {
        std::cout << "0x" 
                  << std::setw(2) << std::setfill('0') << std::hex 
                  << (int)extradata[i] << " ";  // Hiển thị mỗi byte dưới dạng hex
        if ((i + 1) % 16 == 0) {  // Sau mỗi 16 byte, xuống dòng để dễ đọc
            std::cout << std::endl;
        }
    }

    std::cout << std::endl;  
}

void LiveStream::liveThread() {
    AVPacket* packet = av_packet_alloc(); 
    int64_t last_pts = AV_NOPTS_VALUE;  // Track last PTS to ensure monotonic increase

    // 1. Decoder: MJPEG
    // const AVCodec* decoder = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
    // if (!decoder) {
    //     LOG(ERROR) << "MJPEG decoder not found";
    //     return;
    // }
    const AVCodec* decoder = avcodec_find_decoder(baseStream->format_ctx->streams[baseStream->video_stream_index]->codecpar->codec_id);
    if (!decoder) {
        LOG(ERROR) << "Failed to find decoder for input.";
        return; 
    }


    decoder_ctx = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(decoder_ctx, baseStream->format_ctx->streams[baseStream->video_stream_index]->codecpar);
    if (avcodec_open2(decoder_ctx, decoder, nullptr) < 0) {
        LOG(ERROR) << "Failed to open MJPEG decoder";
        return;
    }

    // 2. Encoder: H246
    const AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!encoder) {
        LOG(ERROR) << "H264 encoder not found";
        return;
    }

    encoder_ctx = avcodec_alloc_context3(encoder);

    uint8_t fps = baseStream->getFps();
    // Lower resolution and bitrate for lightweight encoding
    encoder_ctx->bit_rate = 5000000; // Lower bitrate: 5000 kbps (360p)
    encoder_ctx->width = decoder_ctx->width;       // Downscale to 640x360 (360p)
    encoder_ctx->height = decoder_ctx->height;
    encoder_ctx->time_base = AVRational{1, fps}; 
    encoder_ctx->framerate = AVRational{fps, 1};
    encoder_ctx->gop_size = 50;      
    encoder_ctx->max_b_frames = 2;   
    encoder_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    encoder_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // Set encoder options for fast encoding
    av_opt_set(encoder_ctx->priv_data, "preset", "ultrafast", 0); // Fastest encoding preset
    av_opt_set(encoder_ctx->priv_data, "tune", "zerolatency", 0); // Low latency
    av_opt_set(encoder_ctx->priv_data, "flags", "+cgop", 0);      // Closed GOP

    if (avcodec_open2(encoder_ctx, encoder, nullptr) < 0) {
        LOG(ERROR) << "Failed to open H264 encoder";
        return;
    }

    if (encoder_ctx->extradata_size > 0) {
        LOG(INFO) << "Encoder extradata size: " << encoder_ctx->extradata_size;
        showExtradata(encoder_ctx->extradata, encoder_ctx->extradata_size);
        parseSPSandPPS(encoder_ctx->extradata, encoder_ctx->extradata_size);
    } else {
        LOG(ERROR) << "Encoder extradata is still empty!";
    }

    if (decoder_ctx->pix_fmt == AV_PIX_FMT_YUVJ420P ||
        decoder_ctx->pix_fmt == AV_PIX_FMT_YUVJ422P ||
        decoder_ctx->pix_fmt == AV_PIX_FMT_YUVJ444P) {
        LOG(WARNING) << "Converting JPEG-based pixel format to YUV420P.";
        decoder_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    }


    // 3. SWS context: MJPEG may not be YUV420P
    sws_ctx = sws_getContext(
        decoder_ctx->width, decoder_ctx->height, decoder_ctx->pix_fmt,
        encoder_ctx->width, encoder_ctx->height, encoder_ctx->pix_fmt,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    frame = av_frame_alloc();
    yuv_frame = av_frame_alloc();
    yuv_frame->format = encoder_ctx->pix_fmt;
    yuv_frame->width = encoder_ctx->width;
    yuv_frame->height = encoder_ctx->height;
    av_image_alloc(yuv_frame->data, yuv_frame->linesize, yuv_frame->width, yuv_frame->height, encoder_ctx->pix_fmt, 32);

    // // Optional: Send SPS/PPS once
    // if (encoder_ctx->extradata && encoder_ctx->extradata_size > 0) {
    //     transport->streamBuffereToChannel(mLabel, encoder_ctx->extradata, encoder_ctx->extradata_size);
    // }

    FILE *output_file = fopen("output.h264", "wb");

    if (encoder_ctx->extradata && encoder_ctx->extradata_size > 0) {
        fwrite(encoder_ctx->extradata, 1, encoder_ctx->extradata_size, output_file);
    }

    while (mRunning) {
        if (av_read_frame(baseStream->format_ctx, packet) >= 0 && (mState == CameraStarted)) {
            if (packet->stream_index == baseStream->video_stream_index) {\
                // video
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
                        if (packet->pts < packet->dts) {
                            packet->pts = packet->dts;
                        }
                        //LOG(INFO) << "[Revice packet size: " << packet->size << " pts:" << packet->pts << " dts:" << packet->dts << "]";
                        log_packet_tb(&encoder_ctx->time_base, packet);
                        if (output_file) {
                            uint8_t *buffer = (uint8_t *)malloc(packet->size);
                            if (buffer) {
                                memcpy(buffer, packet->data, packet->size);  
                                fwrite(buffer, 1, packet->size, output_file); 
                                free(buffer); 
                            } else {
                                LOG(ERROR) << "Failed to allocate memory for buffer!";
                            }
                        }
                        av_packet_unref(packet);
                    }
                }
            }
            av_packet_unref(packet);
            }
        }
    }
}


Result LiveStream::stream(std::shared_ptr<P2P> p2p, std::string label){
    CAMERA_ASSERT(mState != CameraClosed);

    mLabel = label;
    transport = p2p;
    mP2P = true;
    return Result::SUCCESS;
}