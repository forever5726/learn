#include "audiodecoder.h"
#include <android/log.h>
#include <libavcodec/avcodec.h>
#include <iostream>

#define LOG_TAG "AudioDecoder"
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))

AudioDecoder::AudioDecoder(AudioProcessingContext& ctx) : ctx_(ctx) {}

bool AudioDecoder::setupDecoder() {
    ctx_.codec = avcodec_find_decoder(ctx_.codec_par->codec_id);
    if (!ctx_.codec) {
        LOGE("找不到解码器");
        return false;
    }

    ctx_.codec_ctx = avcodec_alloc_context3(ctx_.codec);
    if (avcodec_parameters_to_context(ctx_.codec_ctx, ctx_.codec_par) < 0) {
        LOGE("无法复制编解码参数");
        return false;
    }

    if (avcodec_open2(ctx_.codec_ctx, ctx_.codec, nullptr) < 0) {
        LOGE("无法打开解码器");
        return false;
    }

    swr_ctx_ = swr_alloc_set_opts(nullptr,
                                  av_get_default_channel_layout(ctx_.codec_ctx->channels),
                                  AV_SAMPLE_FMT_S16,
                                  ctx_.codec_ctx->sample_rate,
                                  av_get_default_channel_layout(ctx_.codec_ctx->channels),
                                  ctx_.codec_ctx->sample_fmt,
                                  ctx_.codec_ctx->sample_rate,
                                  0, nullptr);
    if (!swr_ctx_ || swr_init(swr_ctx_) < 0) {
        LOGE("初始化音频重采样器失败");
        return false;
    }

    LOGI("解码器初始化完成 采样率: %d 通道数: %d",
         ctx_.codec_ctx->sample_rate, ctx_.codec_ctx->channels);
    return true;
}

void AudioDecoder::decode(PacketQueue<AVPacket*>& packetQueue, RingBuffer<uint8_t>& ringBuffer) {
    LOGI("开始解码音频");
    AVFrame* frame = av_frame_alloc();
    uint8_t* convertedData = nullptr;
    int maxOutputSamples = 0;
    int packetCount = 0;
    LOGI("音频队列的总长度%d", packetQueue.size());

    while (!ctx_.decoding_completed || packetQueue.size() > 0) {
        AVPacket* pkt = packetQueue.pop();
        if (!pkt) {
            continue;
        }

        packetCount++;
        LOGI("取出一条音频数据 (总数: %d)", packetCount);

        if (avcodec_send_packet(ctx_.codec_ctx, pkt) < 0) {
            LOGE("发送 packet 到解码器失败");
            av_packet_free(&pkt);
            continue;
        }
        av_packet_free(&pkt);

        while (true) {
            int ret = avcodec_receive_frame(ctx_.codec_ctx, frame);
            if (ret == AVERROR(EAGAIN)) {
                break;
            } else if (ret == AVERROR_EOF) {
                LOGI("解码完成");
                ctx_.decoding_completed = true;
                break;
            } else if (ret < 0) {
                LOGE("解码帧出错");
                break;
            }

            if (frame->nb_samples > maxOutputSamples) {
                maxOutputSamples = frame->nb_samples;
                if (convertedData) {
                    av_freep(&convertedData);
                }

                ret = av_samples_alloc(&convertedData, nullptr,
                                       ctx_.codec_ctx->channels,
                                       maxOutputSamples,
                                       AV_SAMPLE_FMT_S16, 0);
                if (ret < 0) {
                    LOGE("av_samples_alloc 分配内存失败");
                    continue;
                }
            }

            if (swr_ctx_ == nullptr) {
                LOGE("重采样上下文未初始化");
                break;
            }

            int convertedSamples = swr_convert(swr_ctx_, &convertedData, maxOutputSamples,
                                               (const uint8_t**)frame->data, frame->nb_samples);
            if (convertedSamples < 0) {
                LOGE("重采样失败");
                continue;
            }

            int dataSize = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) *
                           convertedSamples * ctx_.codec_ctx->channels;

            size_t bytesWritten = ringBuffer.write(convertedData, dataSize);
            LOGI("缓冲区写入数据+1");
            if (bytesWritten != dataSize) {
                LOGI("环形缓冲区写入: %zu/%d 字节 (缓冲区可能已满)", bytesWritten, dataSize);
            }
        }
    }

    LOGI("解码音频完成, 共处理 %d 个包", packetCount);

    if (convertedData) {
        av_freep(&convertedData);
    }
    av_frame_free(&frame);
    swr_free(&swr_ctx_);
    ctx_.decoding_completed = true;
}