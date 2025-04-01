// videorender.h
#ifndef VIDEORENDER_H
#define VIDEORENDER_H

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "queue.h"
#include <atomic>

extern "C" {
#include <libavutil/frame.h>
}

class VideoRender {
public:
    VideoRender(PacketQueue<AVFrame*>& frameQueue);
    ~VideoRender();

    bool Init(ANativeWindow* window);
    void RenderLoop(ANativeWindow* window);
    void Stop();

private:
    PacketQueue<AVFrame*>& frameQueue_;
    ANativeWindow* window_;
    EGLDisplay display_;
    EGLSurface surface_;
    EGLContext context_;
    std::atomic<bool> running_;
    GLuint program_;
    GLuint positionHandle_;
    GLuint textureHandle_;
    GLuint textureId_[3];  // 用于存储Y、U、V三个纹理的ID

    bool InitEGL();
    bool InitShaders();
    void DrawFrame(AVFrame* frame);
};

#endif // VIDEORENDER_H