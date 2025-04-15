#include <stdio.h>
#include <iostream>

#define __STDC_CONSTANT_MACROS

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
}

int main(int argc, char* argv[]) {
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVPacket *pkt = av_packet_alloc();
    int ret, i;
    int videoindex = -1;
    int frame_index = 0;
    int64_t start_time = 0;

    // Input URL or file
    const char *in_filename = "https://pull-f5-tt03.tiktokcdn.com/game/stream-3287609553826874245_uhd.flv?_session_id=053-20250414083723ED2FDEE6D8F91B34DE8F.1744591055878&_webnoredir=1&expire=1745800643&sign=4ca11206a1da465e4ba7c01d8db84a93";
    const char *out_filename = "rtmp://live-lax.twitch.tv/app/live_1072101235_ztWGwxq7oMHGHkVmsrbqDIGvTV5DW2";

    // Initialize FFmpeg
    avformat_network_init();

    // Open input
    AVDictionary *options = NULL;
    av_dict_set(&options, "rw_timeout", "5000000", 0); // 5 second timeout
    av_dict_set(&options, "reconnect", "1", 0); // Enable reconnection
    av_dict_set(&options, "reconnect_at_eof", "1", 0);
    av_dict_set(&options, "reconnect_streamed", "1", 0);
    av_dict_set(&options, "reconnect_delay_max", "5", 0); // 5 seconds max delay

    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, NULL, &options)) < 0) {
        std::cerr << "Could not open input: " << in_filename << std::endl;
        goto end;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        std::cerr << "Failed to retrieve stream info" << std::endl;
        goto end;
    }

    // Find video stream
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoindex = i;
            break;
        }
    }

    if (videoindex == -1) {
        std::cerr << "No video stream found" << std::endl;
        goto end;
    }

    // Allocate output context
    if ((ret = avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", out_filename)) < 0) {
        std::cerr << "Could not create output context" << std::endl;
        goto end;
    }

    // Copy streams
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) {
            std::cerr << "Failed allocating output stream" << std::endl;
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        // Use codecpar instead of deprecated codec context
        ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
        if (ret < 0) {
            std::cerr << "Failed to copy codec parameters" << std::endl;
            goto end;
        }
    }

    // Open output
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if ((ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE)) < 0) {
            std::cerr << "Could not open output URL: " << out_filename << std::endl;
            goto end;
        }
    }

    // Write header
    if ((ret = avformat_write_header(ofmt_ctx, NULL)) < 0) {
        std::cerr << "Error writing header" << std::endl;
        goto end;
    }

    start_time = av_gettime();
    while (1) {
        AVStream *in_stream, *out_stream;
        
        ret = av_read_frame(ifmt_ctx, pkt);
        if (ret < 0) {
            // Handle end of file or network errors
            if (ret == AVERROR_EOF) {
                std::cout << "End of stream" << std::endl;
                break;
            } else {
                std::cerr << "Error reading frame: " << ret << std::endl;
                break;
            }
        }

        in_stream = ifmt_ctx->streams[pkt->stream_index];
        out_stream = ofmt_ctx->streams[pkt->stream_index];

        // Handle timestamps
        if (pkt->pts == AV_NOPTS_VALUE) {
            AVRational time_base = in_stream->time_base;
            int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
            pkt->pts = (double)(frame_index * calc_duration) / (double)(av_q2d(time_base) * AV_TIME_BASE);
            pkt->dts = pkt->pts;
            pkt->duration = (double)calc_duration / (double)(av_q2d(time_base) * AV_TIME_BASE);
        }

        // Convert timestamps
        pkt->pts = av_rescale_q_rnd(pkt->pts, in_stream->time_base, out_stream->time_base, 
                                   (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt->dts = av_rescale_q_rnd(pkt->dts, in_stream->time_base, out_stream->time_base, 
                                   (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt->duration = av_rescale_q(pkt->duration, in_stream->time_base, out_stream->time_base);
        pkt->pos = -1;

        // Write packet
        if ((ret = av_interleaved_write_frame(ofmt_ctx, pkt)) < 0) {
            std::cerr << "Error writing packet" << std::endl;
            break;
        }

        if (pkt->stream_index == videoindex) {
            std::cout << "Sent frame " << frame_index++ << std::endl;
        }

        av_packet_unref(pkt);
    }

    // Write trailer
    av_write_trailer(ofmt_ctx);

end:
    // Cleanup
    av_packet_free(&pkt);
    if (ifmt_ctx) avformat_close_input(&ifmt_ctx);
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_close(ofmt_ctx->pb);
    }
    if (ofmt_ctx) {
        avformat_free_context(ofmt_ctx);
    }
    av_dict_free(&options);

    return ret < 0 ? 1 : 0;
}