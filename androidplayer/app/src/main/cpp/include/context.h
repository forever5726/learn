#ifndef VIDEO_PROCESSING_CONTEXT_H
#define VIDEO_PROCESSING_CONTEXT_H

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

struct VideoProcessingContext {
    // 解复用相关
    AVFormatContext* format_ctx = nullptr;
    int video_stream_idx = -1;
    int audio_stream_idx = -1;
    AVCodecParameters* codec_par = nullptr;

    // 新增：音频编解码器参数
    AVCodecParameters* audio_codec_par = nullptr;

    // 解码相关
    AVCodecContext* codec_ctx = nullptr;
    const AVCodec* codec = nullptr;

    // 新增：音频解码相关
    AVCodecContext* audio_codec_ctx = nullptr;
    const AVCodec* audio_codec = nullptr;

    // 状态控制
    bool demuxing_completed = false;
    bool decoding_completed = false;

    // 资源清理
    ~VideoProcessingContext() {
        if (codec_ctx) avcodec_free_context(&codec_ctx);
        if (format_ctx) avformat_close_input(&format_ctx);
        // 新增：释放音频解码上下文
        if (audio_codec_ctx) avcodec_free_context(&audio_codec_ctx);
    }

    // 新增：构造函数用于初始化
    VideoProcessingContext() {
        format_ctx = avformat_alloc_context();
    }
};

#endif