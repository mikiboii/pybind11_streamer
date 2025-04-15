#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/functional.h>

#include <iostream>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

#include <libavutil/mathematics.h>
#include <libavutil/time.h>
}

namespace py = pybind11;

class Stream_demo {
public:
    Stream_demo(const std::string &in_filename, const std::string &out_filename)  {

        // in_filename = input_url.c_str();
        // out_filename = output_url.c_str();
                

        // std::cout << "print urls:" << in_filename << " : " << out_filename << std::endl;



        //  AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
        // AVPacket *pkt = av_packet_alloc();
        // int ret, i;
        // int videoindex = -1;
        // int frame_index = 0;
        // int64_t start_time = 0;

        // Input URL or file
        // const char *in_filename = "https://pull-f5-tt03.tiktokcdn.com/game/stream-3287609553826874245_uhd.flv?_session_id=053-20250414083723ED2FDEE6D8F91B34DE8F.1744591055878&_webnoredir=1&expire=1745800643&sign=4ca11206a1da465e4ba7c01d8db84a93";
        // const char *out_filename = "rtmp://live-lax.twitch.tv/app/live_1072101235_ztWGwxq7oMHGHkVmsrbqDIGvTV5DW2";

        // Initialize FFmpeg
        avformat_network_init();
        pkt = av_packet_alloc();

        // Open input
        // AVDictionary *options = NULL;

        av_dict_set(&options, "rw_timeout", "5000000", 0);
        av_dict_set(&options, "reconnect", "1", 0);
        av_dict_set(&options, "reconnect_at_eof", "1", 0);
        av_dict_set(&options, "reconnect_streamed", "1", 0);
        av_dict_set(&options, "reconnect_delay_max", "5", 0);

        // Open input
        if (avformat_open_input(&ifmt_ctx, in_filename.c_str(), nullptr, &options) < 0) {
            throw std::runtime_error("Could not open input: " + in_filename);
        }

        if (avformat_find_stream_info(ifmt_ctx, nullptr) < 0) {
            throw std::runtime_error("Failed to retrieve stream info");
        }

        // Find video stream
        for (unsigned int i = 0; i < ifmt_ctx->nb_streams; i++) {
            if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoindex = i;
                break;
            }
        }

        if (videoindex == -1) {
            throw std::runtime_error("No video stream found");
        }

        // Allocate output context
        if (avformat_alloc_output_context2(&ofmt_ctx, nullptr, "flv", out_filename.c_str()) < 0) {
            throw std::runtime_error("Could not create output context");
        }

        // Copy streams
        for (unsigned int i = 0; i < ifmt_ctx->nb_streams; i++) {
            AVStream *in_stream = ifmt_ctx->streams[i];
            AVStream *out_stream = avformat_new_stream(ofmt_ctx, nullptr);
            if (!out_stream) {
                throw std::runtime_error("Failed allocating output stream");
            }

            if (avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar) < 0) {
                throw std::runtime_error("Failed to copy codec parameters");
            }
        }

        // Open output
        if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&ofmt_ctx->pb, out_filename.c_str(), AVIO_FLAG_WRITE) < 0) {
                throw std::runtime_error("Could not open output URL: " + out_filename);
            }
        }

        // Write header
        if (avformat_write_header(ofmt_ctx, nullptr) < 0) {
            throw std::runtime_error("Error writing header");
        }

        initialized = true;
        // return true;
    }

    ~Stream_demo() {
        // avcodec_close(codec_ctx);
        // avcodec_free_context(&codec_ctx);
        // av_parser_close(parser);
        // output_height = 0;
         if (ofmt_ctx && initialized) {
            av_write_trailer(ofmt_ctx);
        }
        if (pkt) av_packet_free(&pkt);
        if (ifmt_ctx) avformat_close_input(&ifmt_ctx);
        if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
            avio_close(ofmt_ctx->pb);
        }
        if (ofmt_ctx) avformat_free_context(ofmt_ctx);
        if (options) av_dict_free(&options);
        
        ifmt_ctx = nullptr;
        ofmt_ctx = nullptr;
        pkt = nullptr;
        options = nullptr;
        initialized = false;

        std::cout << "closing app" << std::endl;
    }

    void set_size(int width, int height) {
        output_width = width;
        output_height = height;
    }

    int send_stream() {
        // std::cout << "send stream" << std::endl;

        if (!initialized) {
            throw std::runtime_error("FFmpegRelay not initialized");
        }

        AVStream *in_stream, *out_stream;
        int ret = av_read_frame(ifmt_ctx, pkt);
        
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                return -1;  // End of stream
            }
            throw std::runtime_error("Error reading frame: " + std::to_string(ret));
        }

        in_stream = ifmt_ctx->streams[pkt->stream_index];
        out_stream = ofmt_ctx->streams[pkt->stream_index];

        // Handle timestamps
        if (pkt->pts == AV_NOPTS_VALUE) {
            AVRational time_base = in_stream->time_base;
            int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
            pkt->pts = (double)(pkt->pts * calc_duration) / (double)(av_q2d(time_base) * AV_TIME_BASE);
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
            throw std::runtime_error("Error writing packet: " + std::to_string(ret));
        }

        av_packet_unref(pkt);
        return pkt->stream_index == videoindex ? 1 : 0;
    }

    // py::array_t<uint8_t> decode(const char* frameData, size_t frameSize) {
    //     std::vector<uint8_t> decoded_frame;
    //     bool got_frame = false;

        
    // }

private:

    // const char *in_filename ;
    // const char *out_filename;
    AVFormatContext *ifmt_ctx = nullptr;
    AVFormatContext *ofmt_ctx = nullptr;
    AVPacket *pkt = nullptr;
    int videoindex = -1;
    AVDictionary *options = nullptr;
    bool initialized = false;

    
    int output_width;
    int output_height;
};

PYBIND11_MODULE(mymodule, m) {
    m.doc() = "example plugin"; // Optional docstring

    py::class_<Stream_demo>(m, "Stream_demo")
        .def(py::init<const std::string&, const std::string&>(), 
             py::arg("input_url"), py::arg("output_url"))
        .def("set_size", &Stream_demo::set_size, "Set the desired output frame size")
        .def("send_stream", &Stream_demo::send_stream, "send stream to server");
}


// g++ -O3 -Wall -shared -std=c++11 -fPIC $(python3 -m pybind11 --includes) class_demo.cpp \
//     -o module$(python3-config --extension-suffix) \
//     -lavformat -lavcodec -lavutil -lswscale



// g++ -O3 -Wall -shared -std=c++11 -fPIC $(python3 -m pybind11 --includes) class_demo.cpp -o mymodule$(python3-config --extension-suffix) \
// -lavformat -lavcodec -lavutil