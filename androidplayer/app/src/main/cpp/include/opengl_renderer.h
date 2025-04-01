//
// Created by lhm on 2025/3/30.
//

#ifndef ANDROIDPLAYER_OPENGL_RENDERER_H
#define ANDROIDPLAYER_OPENGL_RENDERER_H


#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/native_window.h>
#include <iostream>
#include <stdexcept>

extern "C" {
#include <libavutil/frame.h>
}

class OpenGLRender {
public:
    OpenGLRender(ANativeWindow* window);
    ~OpenGLRender();

    bool init();
    bool renderFrame(AVFrame* frame);

private:
    ANativeWindow* mNativeWindow;
    EGLDisplay mEglDisplay;
    EGLContext mEglContext;
    EGLSurface mEglSurface;
    GLuint mProgram;
    GLuint mTextureY;
    GLuint mTextureU;
    GLuint mTextureV;
    GLint mPositionHandle;
    GLint mTexCoordHandle;
    GLint mSamplerYHandle;
    GLint mSamplerUHandle;
    GLint mSamplerVHandle;

    bool initEGL();
    bool initShaders();
    bool initTextures();
};


#endif //ANDROIDPLAYER_OPENGL_RENDERER_H
