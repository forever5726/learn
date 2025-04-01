#include "demuxer.h"
#include <android/log.h>
#define TAG "Demuxer"
#include <chrono>
#include <thread>
Demuxer::Demuxer(VideoProcessingContext& ctx, AudioProcessingContext& audioctx)
        : ctx_(ctx), audio_ctx_(audioctx) {}

bool Demuxer::openInput(const char* url) {
    if (avformat_open_input(&ctx_.format_ctx, url, nullptr, nullptr) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "无法打开输入文件");
        return false;
    }

    if (avformat_find_stream_info(ctx_.format_ctx, nullptr) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "无法获取流信息");
        return false;
    }

    // 查找视频流
    for (unsigned i = 0; i < ctx_.format_ctx->nb_streams; i++) {
        if (ctx_.format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            ctx_.video_stream_idx = i;
            ctx_.codec_par = ctx_.format_ctx->streams[i]->codecpar;

            // 查找视频解码器
            ctx_.codec = avcodec_find_decoder(ctx_.codec_par->codec_id);
            if (!ctx_.codec) {
                __android_log_print(ANDROID_LOG_ERROR, TAG, "未找到视频解码器");
                return false;
            }

            // 分配解码器上下文
            ctx_.codec_ctx = avcodec_alloc_context3(ctx_.codec);
            if (!ctx_.codec_ctx) {
                __android_log_print(ANDROID_LOG_ERROR, TAG, "无法分配视频解码器上下文");
                return false;
            }

            // 从编解码器参数复制到解码器上下文
            if (avcodec_parameters_to_context(ctx_.codec_ctx, ctx_.codec_par) < 0) {
                __android_log_print(ANDROID_LOG_ERROR, TAG, "无法复制视频编解码器参数到上下文");
                avcodec_free_context(&ctx_.codec_ctx);
                return false;
            }

            // 打开解码器
            if (avcodec_open2(ctx_.codec_ctx, ctx_.codec, nullptr) < 0) {
                __android_log_print(ANDROID_LOG_ERROR, TAG, "无法打开视频解码器");
                avcodec_free_context(&ctx_.codec_ctx);
                return false;
            }

            __android_log_print(ANDROID_LOG_INFO, TAG, "找到视频流索引: %d", i);
            break;
        }
    }

    if (ctx_.video_stream_idx == -1) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "未找到视频流");
        return false;
    }

    return true;
}

void Demuxer::start(PacketQueue<AVPacket*>& packetQueue) {
    AVPacket* pkt = av_packet_alloc();
    while (!ctx_.demuxing_completed) {
        if (av_read_frame(ctx_.format_ctx, pkt) < 0) {
            __android_log_print(ANDROID_LOG_INFO, TAG, "解复用视频完成");
            break;
        }
        __android_log_print(ANDROID_LOG_INFO, TAG, "添加一条消息");
        if (pkt->stream_index == ctx_.video_stream_idx) {
            AVPacket* cloned = av_packet_clone(pkt);
            packetQueue.push(cloned);
        }
        av_packet_unref(pkt);
    }
    ctx_.demuxing_completed = true;
    av_packet_free(&pkt);
}

// 新增方法：打开输入文件时查找音频流
bool Demuxer::openInputWithAudio(const char* url) {
    if (!openInput(url)) {
        return false;
    }

    // 查找音频流
    for (unsigned i = 0; i < ctx_.format_ctx->nb_streams; i++) {
        if (ctx_.format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_ctx_.audio_stream_idx = i;  // 修改为写入 audio_ctx_
            audio_ctx_.codec_par = ctx_.format_ctx->streams[i]->codecpar;  // 修改为写入 audio_ctx_

            // 查找音频解码器
            audio_ctx_.codec = avcodec_find_decoder(audio_ctx_.codec_par->codec_id);  // 修改为写入 audio_ctx_
            if (!audio_ctx_.codec) {
                __android_log_print(ANDROID_LOG_ERROR, TAG, "未找到音频解码器");
                return false;
            }

            // 分配音频解码器上下文
            audio_ctx_.codec_ctx = avcodec_alloc_context3(audio_ctx_.codec);  // 修改为写入 audio_ctx_
            if (!audio_ctx_.codec_ctx) {
                __android_log_print(ANDROID_LOG_ERROR, TAG, "无法分配音频解码器上下文");
                return false;
            }

            // 从编解码器参数复制到音频解码器上下文
            if (avcodec_parameters_to_context(audio_ctx_.codec_ctx, audio_ctx_.codec_par) < 0) {
                __android_log_print(ANDROID_LOG_ERROR, TAG, "无法复制音频编解码器参数到上下文");
                avcodec_free_context(&audio_ctx_.codec_ctx);
                return false;
            }

            // 打开音频解码器
            if (avcodec_open2(audio_ctx_.codec_ctx, audio_ctx_.codec, nullptr) < 0) {
                __android_log_print(ANDROID_LOG_ERROR, TAG, "无法打开音频解码器");
                avcodec_free_context(&audio_ctx_.codec_ctx);
                return false;
            }

            __android_log_print(ANDROID_LOG_INFO, TAG, "找到音频流索引: %d", i);
            break;
        }
    }

    if (audio_ctx_.audio_stream_idx == -1) {  // 修改为检查 audio_ctx_ 中的索引
        __android_log_print(ANDROID_LOG_ERROR, TAG, "未找到音频流");
        return false;
    }

    return true;
}

// 新增方法：开始解复用视频和音频
void Demuxer::startWithAudio(PacketQueue<AVPacket*>& videoPacketQueue, PacketQueue<AVPacket*>& audioPacketQueue) {
    AVPacket* pkt = av_packet_alloc();
    while (!ctx_.demuxing_completed) {
        if (av_read_frame(ctx_.format_ctx, pkt) < 0) {
            __android_log_print(ANDROID_LOG_INFO, TAG, "解复用视频和音频完成");
            break;
        }

        __android_log_print(ANDROID_LOG_INFO, TAG, "添加一条消息");
        if (pkt->stream_index == ctx_.video_stream_idx) {
            AVPacket* cloned = av_packet_clone(pkt);
            videoPacketQueue.push(cloned);
        } else if (pkt->stream_index == audio_ctx_.audio_stream_idx) {  // 修改为检查 audio_ctx_ 中的索引
            AVPacket* cloned = av_packet_clone(pkt);
            audioPacketQueue.push(cloned);
        }
        av_packet_unref(pkt);
    }
    __android_log_print(ANDROID_LOG_INFO, "VideoPacketQueue", "队列长度: %d", audioPacketQueue.size());
    //ctx_.demuxing_completed = true;
    av_packet_free(&pkt);
}