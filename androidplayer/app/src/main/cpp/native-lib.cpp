#include <jni.h>
#include "demuxer.h"
#include "videodecoder.h"
#include "queue.h"
#include "videorender.h"
#include "opengl_renderer.h"
#include "audiodecoder.h"
#include "AAudioRender.h"
#include "RingBuffer.h"
#include <thread>
#include <android/log.h>
#include <unistd.h>
#include <sys/stat.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#define LOG_TAG "VideoProcessor"
#include <iostream>



extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_androidplayer_MainActivity_processVideo(
        JNIEnv* env, jobject thiz,
        jstring input_path, jstring output_path, jobject surface) {
    if (surface == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Surface is null");
        return JNI_FALSE;
    }
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Surface is  ok");
    // 获取输入文件路径
    const char* input_path_str = env->GetStringUTFChars(input_path, nullptr);
    if (!input_path_str) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "获取输入路径失败");
        return JNI_FALSE;
    }

    // 1. 检查文件是否存在
    if (access(input_path_str, F_OK) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "文件不存在: %s", input_path_str);
        env->ReleaseStringUTFChars(input_path, input_path_str);
        return JNI_FALSE;
    }

    // 2. 检查文件是否可读
    if (access(input_path_str, R_OK) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "文件不可读: %s", input_path_str);
        env->ReleaseStringUTFChars(input_path, input_path_str);
        return JNI_FALSE;
    }

    // 3. 检查文件大小
    struct stat file_stat;
    if (stat(input_path_str, &file_stat) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "获取文件信息失败: %s", input_path_str);
        env->ReleaseStringUTFChars(input_path, input_path_str);
        return JNI_FALSE;
    }

    if (file_stat.st_size <= 0) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "文件大小为0: %s", input_path_str);
        env->ReleaseStringUTFChars(input_path, input_path_str);
        return JNI_FALSE;
    }

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "文件检查通过: %s (大小: %lld字节)",
                        input_path_str, (long long)file_stat.st_size);
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "获取文件: %s", input_path_str);
    // 初始化上下文
    VideoProcessingContext ctx;
    AudioProcessingContext audioctx;
    // 设置解复用器
    Demuxer demuxer(ctx, audioctx);
    if (!demuxer.openInputWithAudio(input_path_str)) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "解复用器（包含音频）初始化失败");
        env->ReleaseStringUTFChars(input_path, input_path_str);
        return JNI_FALSE;
    }

    // 设置视频解码器
    VideoDecoder decoder(ctx);
    if (!decoder.setupDecoder()) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "解码器初始化失败");
        env->ReleaseStringUTFChars(input_path, input_path_str);
        return JNI_FALSE;
    }
    // 设置音频解码器
    AudioDecoder audioDecoder(audioctx);
    if (!audioDecoder.setupDecoder()) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "音频解码器初始化失败");
        env->ReleaseStringUTFChars(input_path, input_path_str);
        return JNI_FALSE;
    }

    // 获取输出路径
    const char* output_path_str = env->GetStringUTFChars(output_path, nullptr);
    if (!output_path_str) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "获取输出路径失败");
        env->ReleaseStringUTFChars(input_path, input_path_str);
        return JNI_FALSE;
    }

    // 创建视频队列
    PacketQueue<AVPacket*> packetQueue;
    PacketQueue<AVFrame*> frameQueue;
    PacketQueue<AVPacket*> packetQueue2;
    // 创建音频队列

    RingBuffer<uint8_t> ringBuffer(4096);
    // 创建视频渲染器
    VideoRender videoRender(frameQueue);

    // 初始化 VideoRender
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "获取 Surface 失败");
        env->ReleaseStringUTFChars(input_path, input_path_str);
        env->ReleaseStringUTFChars(output_path, output_path_str);
        return JNI_FALSE;
    }
    // 初始化音频渲染器
    // 创建 AAudioRender 实例
    AAudioRender audioRender;
    audioRender.setCallback(AAudioRender::myAudioCallback, &ringBuffer);
    audioRender.configure(44100, 2, AAUDIO_FORMAT_PCM_I16);

    audioRender.start();
    // 启动线程
    // 解复用线程
    std::thread demux_thread([&] {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "开始解复用线程");
        demuxer.startWithAudio(packetQueue, packetQueue2);
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "解复用线程完成");
    });
    // 视频解码线程
    // 视频渲染线程
    std::thread render_thread([&] {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "开始渲染线程");
        videoRender.RenderLoop(window);
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "渲染线程完成");
    });
    std::thread decode_thread([&] {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "开始解码线程");
        decoder.decode(packetQueue, frameQueue, window);
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "解码线程完成");
    });
    // 音频解码线程

    std::thread audio_decode_thread([&] {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "开始音频解码线程");
        audioDecoder.decode(packetQueue2, ringBuffer);
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "解码音频线程完成");
    });


    // 等待完成
    audio_decode_thread.join();
    demux_thread.join();
    decode_thread.join();
    videoRender.Stop();
    render_thread.join();


    __android_log_print(ANDROID_LOG_INFO, "PacketQueue", "外部: %zu", packetQueue2.size());
    // 释放资源
    ANativeWindow_release(window);
    env->ReleaseStringUTFChars(input_path, input_path_str);
    env->ReleaseStringUTFChars(output_path, output_path_str);

    // 验证结果
    bool success = (ctx.demuxing_completed && ctx.decoding_completed);
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "处理完成: %s",
                        success? "成功" : "失败");

    return success? JNI_TRUE : JNI_FALSE;
}