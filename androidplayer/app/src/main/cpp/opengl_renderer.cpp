#include "opengl_renderer.h"
#include <android/log.h>

// 顶点着色器代码
const char* vertexShaderSource =
        "attribute vec4 aPosition;\n"
        "attribute vec2 aTexCoord;\n"
        "varying vec2 vTexCoord;\n"
        "void main() {\n"
        "    gl_Position = aPosition;\n"
        "    vTexCoord = aTexCoord;\n"
        "}\n";

// 片段着色器代码
const char* fragmentShaderSource =
        "#extension GL_OES_EGL_image_external : require\n"
        "precision mediump float;\n"
        "varying vec2 vTexCoord;\n"
        "uniform sampler2D sTextureY;\n"
        "uniform sampler2D sTextureU;\n"
        "uniform sampler2D sTextureV;\n"
        "void main() {\n"
        "    float y = texture2D(sTextureY, vTexCoord).r;\n"
        "    float u = texture2D(sTextureU, vTexCoord).r - 0.5;\n"
        "    float v = texture2D(sTextureV, vTexCoord).r - 0.5;\n"
        "    vec3 rgb;\n"
        "    rgb.r = y + 1.402 * v;\n"
        "    rgb.g = y - 0.344 * u - 0.714 * v;\n"
        "    rgb.b = y + 1.772 * u;\n"
        "    gl_FragColor = vec4(rgb, 1.0);\n"
        "}\n";

#define LOG_TAG "OpenGLRender"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

OpenGLRender::OpenGLRender(ANativeWindow* window)
        : mNativeWindow(window), mEglDisplay(EGL_NO_DISPLAY), mEglContext(EGL_NO_CONTEXT), mEglSurface(EGL_NO_SURFACE),
          mProgram(0), mTextureY(0), mTextureU(0), mTextureV(0) {}

OpenGLRender::~OpenGLRender() {
    if (mEglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(mEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (mEglContext != EGL_NO_CONTEXT) {
            eglDestroyContext(mEglDisplay, mEglContext);
        }
        if (mEglSurface != EGL_NO_SURFACE) {
            eglDestroySurface(mEglDisplay, mEglSurface);
        }
        eglTerminate(mEglDisplay);
    }
    if (mProgram != 0) {
        glDeleteProgram(mProgram);
    }
    if (mTextureY != 0) {
        glDeleteTextures(1, &mTextureY);
    }
    if (mTextureU != 0) {
        glDeleteTextures(1, &mTextureU);
    }
    if (mTextureV != 0) {
        glDeleteTextures(1, &mTextureV);
    }
}

bool OpenGLRender::init() {
    if (!initEGL()) {
        return false;
    }
    if (!initShaders()) {
        return false;
    }
    if (!initTextures()) {
        return false;
    }
    return true;
}

bool OpenGLRender::initEGL() {
    mEglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (mEglDisplay == EGL_NO_DISPLAY) {
        LOGE("Failed to get EGL display");
        return false;
    }

    EGLint majorVersion, minorVersion;
    if (!eglInitialize(mEglDisplay, &majorVersion, &minorVersion)) {
        LOGE("Failed to initialize EGL");
        return false;
    }

    const EGLint configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NONE
    };

    EGLConfig eglConfig;
    EGLint numConfigs;
    if (!eglChooseConfig(mEglDisplay, configAttribs, &eglConfig, 1, &numConfigs)) {
        LOGE("Failed to choose EGL config");
        return false;
    }

    mEglSurface = eglCreateWindowSurface(mEglDisplay, eglConfig, mNativeWindow, nullptr);
    if (mEglSurface == EGL_NO_SURFACE) {
        LOGE("Failed to create EGL surface");
        return false;
    }

    const EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
    };

    mEglContext = eglCreateContext(mEglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttribs);
    if (mEglContext == EGL_NO_CONTEXT) {
        LOGE("Failed to create EGL context");
        return false;
    }

    if (!eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext)) {
        LOGE("Failed to make EGL context current");
        return false;
    }

    return true;
}

bool OpenGLRender::initShaders() {
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    GLint compiled;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        LOGE("Failed to compile vertex shader");
        glDeleteShader(vertexShader);
        return false;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        LOGE("Failed to compile fragment shader");
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    mProgram = glCreateProgram();
    glAttachShader(mProgram, vertexShader);
    glAttachShader(mProgram, fragmentShader);
    glLinkProgram(mProgram);

    GLint linked;
    glGetProgramiv(mProgram, GL_LINK_STATUS, &linked);
    if (!linked) {
        LOGE("Failed to link shader program");
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(mProgram);
        return false;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    mPositionHandle = glGetAttribLocation(mProgram, "aPosition");
    mTexCoordHandle = glGetAttribLocation(mProgram, "aTexCoord");
    mSamplerYHandle = glGetUniformLocation(mProgram, "sTextureY");
    mSamplerUHandle = glGetUniformLocation(mProgram, "sTextureU");
    mSamplerVHandle = glGetUniformLocation(mProgram, "sTextureV");

    return true;
}

bool OpenGLRender::initTextures() {
    glGenTextures(1, &mTextureY);
    glBindTexture(GL_TEXTURE_2D, mTextureY);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &mTextureU);
    glBindTexture(GL_TEXTURE_2D, mTextureU);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &mTextureV);
    glBindTexture(GL_TEXTURE_2D, mTextureV);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return true;
}

bool OpenGLRender::renderFrame(AVFrame* frame) {
    if (!frame) {
        return false;
    }

    glUseProgram(mProgram);

    // 更新纹理数据
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTextureY);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, frame->width, frame->height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, frame->data[0]);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, mTextureU);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, frame->width / 2, frame->height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, frame->data[1]);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, mTextureV);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, frame->width / 2, frame->height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, frame->data[2]);

    // 设置纹理采样器
    glUniform1i(mSamplerYHandle, 0);
    glUniform1i(mSamplerUHandle, 1);
    glUniform1i(mSamplerVHandle, 2);

    // 顶点坐标
    GLfloat vertices[] = {
            -1.0f, -1.0f,
            1.0f, -1.0f,
            -1.0f,  1.0f,
            1.0f,  1.0f
    };
    glVertexAttribPointer(mPositionHandle, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(mPositionHandle);

    // 纹理坐标
    GLfloat texCoords[] = {
            0.0f, 1.0f,
            1.0f, 1.0f,
            0.0f, 0.0f,
            1.0f, 0.0f
    };
    glVertexAttribPointer(mTexCoordHandle, 2, GL_FLOAT, GL_FALSE, 0, texCoords);
    glEnableVertexAttribArray(mTexCoordHandle);

    // 绘制
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // 交换缓冲区
    eglSwapBuffers(mEglDisplay, mEglSurface);

    return true;
}