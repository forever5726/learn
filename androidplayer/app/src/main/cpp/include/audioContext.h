#ifndef AUDIO_PROCESSING_CONTEXT_H
#define AUDIO_PROCESSING_CONTEXT_H

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

struct AudioProcessingContext {
    // 解复用相关
    AVFormatContext* format_ctx = nullptr;
    int audio_stream_idx = -1;
    AVCodecParameters* codec_par = nullptr;

    // 解码相关
    AVCodecContext* codec_ctx = nullptr;
    const AVCodec* codec = nullptr;

    // 状态控制
    bool demuxing_completed = false;
    bool decoding_completed = false;

    // 资源清理
    ~AudioProcessingContext() {
        if (codec_ctx) avcodec_free_context(&codec_ctx);
        if (format_ctx) avformat_close_input(&format_ctx);
    }

    // 构造函数用于初始化
    AudioProcessingContext() {
        format_ctx = avformat_alloc_context();
    }
};

#endif