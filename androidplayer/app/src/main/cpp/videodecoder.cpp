#include "videodecoder.h"
#include "opengl_renderer.h"

#include <android/log.h>
#include <unistd.h>
extern "C" {
#include "libavutil/imgutils.h"
}
#define TAG "Decoder"

VideoDecoder::VideoDecoder(VideoProcessingContext& ctx) : ctx_(ctx) {}

bool VideoDecoder::setupDecoder() {
    ctx_.codec = avcodec_find_decoder(ctx_.codec_par->codec_id);
    if (!ctx_.codec) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "找不到解码器");
        return false;
    }

    ctx_.codec_ctx = avcodec_alloc_context3(ctx_.codec);
    if (avcodec_parameters_to_context(ctx_.codec_ctx, ctx_.codec_par) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "无法复制编解码参数");
        return false;
    }

    if (avcodec_open2(ctx_.codec_ctx, ctx_.codec, nullptr) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "无法打开解码器");
        return false;
    }

    // 初始化图像转换器
    sws_ctx_ = sws_getContext(
            ctx_.codec_ctx->width, ctx_.codec_ctx->height, ctx_.codec_ctx->pix_fmt,
            ctx_.codec_ctx->width, ctx_.codec_ctx->height, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    __android_log_print(ANDROID_LOG_INFO, TAG, "解码器初始化完成 %dx%d",
                        ctx_.codec_ctx->width, ctx_.codec_ctx->height);
    return true;
}

void VideoDecoder::decode(PacketQueue<AVPacket*>& packetQueue, PacketQueue<AVFrame*>& frameQueue, ANativeWindow* window) {
    AVFrame* frame = av_frame_alloc();
    SwsContext* sws_ctx = nullptr;


    while (!ctx_.decoding_completed) {
        AVPacket* pkt = packetQueue.pop();
        if (!pkt && ctx_.demuxing_completed) {
            __android_log_print(ANDROID_LOG_INFO, TAG, "解码完成");
            break;
        }

        // 发送数据包到解码器
        int send_ret = avcodec_send_packet(ctx_.codec_ctx, pkt);
        av_packet_free(&pkt);

        if (send_ret < 0 && send_ret != AVERROR(EAGAIN)) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "发送Packet失败: %d", send_ret);
            continue;
        }

        // 接收解码后的帧
        while (true) {
            int recv_ret = avcodec_receive_frame(ctx_.codec_ctx, frame);
            if (recv_ret == AVERROR(EAGAIN) || recv_ret == AVERROR_EOF) break;
            else if (recv_ret < 0) {
                __android_log_print(ANDROID_LOG_ERROR, TAG, "接收Frame失败: %d", recv_ret);
                break;
            }

            // 确保目标格式是YUV420P
            AVFrame* yuv420p_frame = av_frame_alloc();
            yuv420p_frame->format = AV_PIX_FMT_YUV420P;
            yuv420p_frame->width = frame->width;
            yuv420p_frame->height = frame->height;

            // 关键修改：显式分配缓冲区并手动填充数据
            if (av_frame_get_buffer(yuv420p_frame, 32) < 0) { // 32字节对齐
                __android_log_print(ANDROID_LOG_ERROR, TAG, "分配YUV帧失败");
                av_frame_free(&yuv420p_frame);
                av_frame_unref(frame);
                continue;
            }

            // 初始化SWS上下文（仅当格式不同时）
            if (!sws_ctx || frame->format != ctx_.codec_ctx->pix_fmt) {
                sws_freeContext(sws_ctx);
                sws_ctx = sws_getContext(
                        frame->width, frame->height, (AVPixelFormat)frame->format,
                        yuv420p_frame->width, yuv420p_frame->height, AV_PIX_FMT_YUV420P,
                        SWS_BILINEAR, nullptr, nullptr, nullptr);
                if (!sws_ctx) {
                    __android_log_print(ANDROID_LOG_ERROR, TAG, "初始化SWS上下文失败");
                    av_frame_free(&yuv420p_frame);
                    av_frame_unref(frame);
                    continue;
                }
            }

            // 执行转换
            sws_scale(sws_ctx,
                      frame->data, frame->linesize, 0, frame->height,
                      yuv420p_frame->data, yuv420p_frame->linesize);

            // 验证数据（调试用）
            __android_log_print(ANDROID_LOG_DEBUG, TAG,
                                "YUV数据: Y[0]=%d, U[0]=%d, V[0]=%d",
                                yuv420p_frame->data[0][0],
                                yuv420p_frame->data[1][0],
                                yuv420p_frame->data[2][0]);

            // 创建帧的副本放入队列
            AVFrame* frame_copy = av_frame_alloc();
            if (av_frame_ref(frame_copy, yuv420p_frame) < 0) {
                __android_log_print(ANDROID_LOG_ERROR, TAG, "创建帧副本失败");
                av_frame_free(&frame_copy);
            } else {
                frameQueue.push(frame_copy);  // 将帧放入队列
            }

//            // 传递帧并渲染
//            if (!renderer.renderFrame(yuv420p_frame)) {
//                __android_log_print(ANDROID_LOG_ERROR, TAG, "渲染帧失败");
//            }

            av_frame_unref(frame);
            av_frame_free(&yuv420p_frame); // 渲染完后释放frame
        }
    }

    av_frame_free(&frame);
    sws_freeContext(sws_ctx);
    ctx_.decoding_completed = true;
}