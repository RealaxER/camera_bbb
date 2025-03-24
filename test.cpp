extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
}
    
#include <iostream>
#include <string>
#include <glog/logging.h>


int main() {
    const char* input_filename = "/dev/video2";
    const char* output_filename = "output.ts";

    // Khởi tạo các biến FFmpeg
    AVFormatContext* input_format_ctx = nullptr;
    AVFormatContext* output_format_ctx = nullptr;
    AVCodecContext* decoder_ctx = nullptr;
    AVCodecContext* encoder_ctx = nullptr;
    AVStream* video_stream = nullptr;

    int video_stream_index = -1;

    
    avdevice_register_all();  // Đăng ký thiết bị video


    AVDictionary* options = nullptr;
    av_dict_set(&options, "format", "mpjpeg", 0);  // Đổi từ "input_format" thành "format"
    av_dict_set(&options, "video_size", "640x480", 0); // Đặt độ phân giải
    av_dict_set(&options, "framerate", "30", 0);        // Đặt frame rate

    AVInputFormat* input_format = av_find_input_format("v4l2");
    if (!input_format) {
        std::cerr << "Failed to find input format 'v4l2'" << std::endl;
        return -1;
    }

    // Mở file input
    int ret = avformat_open_input(&input_format_ctx, input_filename, input_format, &options); 
    if (ret < 0) {
        std::cerr << "Can't open input ret: " << ret << std::endl;
        return -1;
    }
    if (avformat_find_stream_info(input_format_ctx, nullptr) < 0) {
        std::cerr << "Fail to find info stream." << std::endl;
        return -1;
    }
    // print all info stream input
    av_dump_format(input_format_ctx, 0, input_filename, 0);

    // Tìm video stream
    // Hay còn gọi là tìm format support 
    // Nếu không có video thì nó chỉ là audio không thôi 
    // Nếu có thể có cả subtiltle và audio và video luôn
    std::cout << "Format number: " << input_format_ctx->nb_streams << std::endl;
    for (unsigned int i = 0; i < input_format_ctx->nb_streams; i++) {
        if (input_format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }
    if (video_stream_index == -1) {
        std::cerr << "Fail to find video stream." << std::endl;
        return -1;
    }

    // Tìm codec decoder MJPEG và tạo codec context
    /*Create decoder for mjpeg from id input*/
    AVCodec* decoder = avcodec_find_decoder(input_format_ctx->streams[video_stream_index]->codecpar->codec_id);
    if (!decoder) {
        std::cerr << "Fait to find decoder for input." << std::endl;
        return -1;
    }
    /*Create context from decoder */
    decoder_ctx = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(decoder_ctx, input_format_ctx->streams[video_stream_index]->codecpar);
    if (avcodec_open2(decoder_ctx, decoder, nullptr) < 0) {
        std::cerr << "Fail to open codec decoder." << std::endl;
        return -1;
    }

    // Tạo output format context cho file MPEG-TS
    avformat_alloc_output_context2(&output_format_ctx, nullptr, "mpegts", output_filename);
    if (!output_format_ctx) {
        std::cerr << "Fail to create output format context." << std::endl;
        return -1;
    }

    // Tìm codec encoder H.264 cho file MPEG-TS
    AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!encoder) {
        std::cerr << "Fail to find encoder H.264." << std::endl;
        return -1;
    }

    // Tạo encoder context cho video stream output
    encoder_ctx = avcodec_alloc_context3(encoder);
    encoder_ctx->bit_rate = 400000;
    encoder_ctx->width = decoder_ctx->width;
    encoder_ctx->height = decoder_ctx->height;
    encoder_ctx->time_base = {1, 25};  // Time base phù hợp với frame rate
    encoder_ctx->framerate = {25, 1};
    encoder_ctx->gop_size = 12;  // Keyframe mỗi 12 frame
    encoder_ctx->max_b_frames = 2;
    encoder_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    if (avcodec_open2(encoder_ctx, encoder, nullptr) < 0) {
        std::cerr << "Fail to open encoder H.264." << std::endl;
        return -1;
    }

    // Thêm video stream vào output file và copy thông tin codec parameters
    video_stream = avformat_new_stream(output_format_ctx, nullptr);
    avcodec_parameters_from_context(video_stream->codecpar, encoder_ctx);
    video_stream->time_base = encoder_ctx->time_base;

    // Mở file output và ghi header
    if (avio_open(&output_format_ctx->pb, output_filename, AVIO_FLAG_WRITE) < 0) {
        std::cerr << "Fail to open file output." << std::endl;
        return -1;
    }
    if (avformat_write_header(output_format_ctx, nullptr) < 0) {
        std::cerr << "Err: Fail to write header for file output." << std::endl;
        return -1;
    }    

    if (decoder_ctx->pix_fmt == AV_PIX_FMT_YUVJ420P ||
        decoder_ctx->pix_fmt == AV_PIX_FMT_YUVJ422P ||
        decoder_ctx->pix_fmt == AV_PIX_FMT_YUVJ444P) {
        decoder_ctx->pix_fmt = AV_PIX_FMT_YUV420P;  // Ép về pixel format hiện đại
    }


    // Bộ chuyển đổi frame từ MJPEG sang YUV420P (phù hợp với H.264 encoder)
    struct SwsContext* sws_ctx = sws_getContext(
        decoder_ctx->width, decoder_ctx->height, decoder_ctx->pix_fmt,
        encoder_ctx->width, encoder_ctx->height, encoder_ctx->pix_fmt,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    // Chuẩn bị frame và packet
    AVFrame* frame = av_frame_alloc();
    AVFrame* yuv_frame = av_frame_alloc();
    yuv_frame->format = encoder_ctx->pix_fmt;
    yuv_frame->width = encoder_ctx->width;
    yuv_frame->height = encoder_ctx->height;
    av_image_alloc(yuv_frame->data, yuv_frame->linesize, yuv_frame->width, yuv_frame->height, encoder_ctx->pix_fmt, 32);

    AVPacket* packet = av_packet_alloc();

    while (av_read_frame(input_format_ctx, packet) >= 0) {
        std::cout << "Read frame...." << std::endl;
        if (packet->stream_index == video_stream_index) {
            // send to app


            std::cout << "Decode frame...." << std::endl;
            avcodec_send_packet(decoder_ctx, packet);
            while (avcodec_receive_frame(decoder_ctx, frame) >= 0) {
                sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, yuv_frame->data, yuv_frame->linesize);
                

                std::cout << "Encode frame...." << std::endl;
                yuv_frame->pts = av_rescale_q(frame->pts, input_format_ctx->streams[video_stream_index]->time_base, encoder_ctx->time_base);

                avcodec_send_frame(encoder_ctx, yuv_frame);
                while (avcodec_receive_packet(encoder_ctx, packet) >= 0) {
                    packet->pts = av_rescale_q(packet->pts, encoder_ctx->time_base, video_stream->time_base);
                    packet->dts = av_rescale_q(packet->dts, encoder_ctx->time_base, video_stream->time_base);
                    av_interleaved_write_frame(output_format_ctx, packet);
                    av_packet_unref(packet);
                }
            }
        }
        av_packet_unref(packet);
    }

    // Ghi packet còn lại trong buffer
    avcodec_send_frame(encoder_ctx, nullptr);
    while (avcodec_receive_packet(encoder_ctx, packet) >= 0) {
        av_interleaved_write_frame(output_format_ctx, packet);
        av_packet_unref(packet);
    }

    av_write_trailer(output_format_ctx);
    std::cout << "Convert done!" << std::endl;

    // Giải phóng tài nguyên
    avcodec_free_context(&encoder_ctx);
    avcodec_free_context(&decoder_ctx);
    avformat_close_input(&input_format_ctx);
    avio_closep(&output_format_ctx->pb);
    avformat_free_context(output_format_ctx);
    av_frame_free(&frame);
    av_frame_free(&yuv_frame);
    av_packet_free(&packet);
    sws_freeContext(sws_ctx);

    return 0;
}