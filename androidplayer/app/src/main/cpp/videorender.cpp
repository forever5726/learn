#include "videorender.h"
#include "opengl_renderer.h"
#include <thread>
#include <libavutil/imgutils.h>
#include <android/log.h>
#define TAG "videorender"

// 顶点着色器代码
const char* vertexShaderSource2 =
        "attribute vec4 aPosition;\n"
        "attribute vec2 aTexCoord;\n"
        "varying vec2 vTexCoord;\n"
        "void main() {\n"
        "    gl_Position = aPosition;\n"
        "    vTexCoord = aTexCoord;\n"
        "}\n";

// 片段着色器代码
const char* fragmentShaderSource2 =
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

VideoRender::VideoRender(PacketQueue<AVFrame*>& frameQueue)
        : frameQueue_(frameQueue),
          window_(nullptr),
          display_(EGL_NO_DISPLAY),
          surface_(EGL_NO_SURFACE),
          context_(EGL_NO_CONTEXT),
          running_(false),
          program_(0),
          positionHandle_(0),
          textureHandle_(0) {
    for (int i = 0; i < 3; ++i) {
        textureId_[i] = 0;
    }
}

VideoRender::~VideoRender() {
    Stop();
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
        }
        if (surface_ != EGL_NO_SURFACE) {
            eglDestroySurface(display_, surface_);
        }
        eglTerminate(display_);
    }
}

bool VideoRender::Init(ANativeWindow* window) {
    window_ = window;
//    if (!initEGL()) {
//        return false;
//    }
//    if (!initShaders()) {
//        return false;
//    }
//    if (!initTextures()) {
//        return false;
//    }
    return true;
}

bool VideoRender::InitEGL() {
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "eglGetDisplay failed");
        return false;
    }

    EGLint major, minor;
    if (!eglInitialize(display_, &major, &minor)) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "eglInitialize failed");
        return false;
    }

    const EGLint configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_NONE
    };

    EGLConfig config;
    EGLint numConfigs;
    if (!eglChooseConfig(display_, configAttribs, &config, 1, &numConfigs)) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "eglChooseConfig failed");
        return false;
    }

    EGLint format;
    if (!eglGetConfigAttrib(display_, config, EGL_NATIVE_VISUAL_ID, &format)) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "eglGetConfigAttrib failed");
        return false;
    }

    ANativeWindow_setBuffersGeometry(window_, 0, 0, format);

    surface_ = eglCreateWindowSurface(display_, config, window_, nullptr);
    if (surface_ == EGL_NO_SURFACE) {
        EGLint error = eglGetError();
        __android_log_print(ANDROID_LOG_ERROR, TAG, "eglCreateWindowSurface failed with error: %x", error);
        return false;
    }

    const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    context_ = eglCreateContext(display_, config, EGL_NO_CONTEXT, contextAttribs);
    if (context_ == EGL_NO_CONTEXT) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "eglCreateContext failed");
        return false;
    }

    if (!eglMakeCurrent(display_, surface_, surface_, context_)) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "eglMakeCurrent failed");
        return false;
    }

    return true;
}

bool VideoRender::InitShaders() {
    // 顶点着色器
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource2, nullptr);
    glCompileShader(vertexShader);

    // 检查顶点着色器编译状态
    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        __android_log_print(ANDROID_LOG_ERROR, TAG, "顶点着色器编译失败: %s", infoLog);
        return false;
    }

    // 片段着色器
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource2, nullptr);
    glCompileShader(fragmentShader);

    // 检查片段着色器编译状态
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        __android_log_print(ANDROID_LOG_ERROR, TAG, "片段着色器编译失败: %s", infoLog);
        return false;
    }

    program_ = glCreateProgram();
    glAttachShader(program_, vertexShader);
    glAttachShader(program_, fragmentShader);
    glLinkProgram(program_);

    // 检查程序链接状态
    glGetProgramiv(program_, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program_, 512, nullptr, infoLog);
        __android_log_print(ANDROID_LOG_ERROR, TAG, "程序链接失败: %s", infoLog);
        return false;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    positionHandle_ = glGetAttribLocation(program_, "aPosition");
    textureHandle_ = glGetAttribLocation(program_, "aTexCoord");
    GLuint textureUniformY = glGetUniformLocation(program_, "uTextureY");
    GLuint textureUniformU = glGetUniformLocation(program_, "uTextureU");
    GLuint textureUniformV = glGetUniformLocation(program_, "uTextureV");

    for (int i = 0; i < 3; ++i) {
        glGenTextures(1, &textureId_[i]);
        glBindTexture(GL_TEXTURE_2D, textureId_[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glUseProgram(program_);
    glUniform1i(textureUniformY, 0);
    glUniform1i(textureUniformU, 1);
    glUniform1i(textureUniformV, 2);

    return true;
}

void VideoRender::RenderLoop(ANativeWindow* window) {
    __android_log_print(ANDROID_LOG_ERROR, TAG, "进入loop");
    OpenGLRender renderer(window);
    renderer.init();
    running_ = true;
    while (running_) {
        AVFrame* frame = frameQueue_.pop();
        renderer.renderFrame(frame);
        __android_log_print(ANDROID_LOG_ERROR, TAG, "获取frame");
        if (frame) {
            DrawFrame(frame);
            av_frame_unref(frame);
            av_frame_free(&frame);
        } else if (frameQueue_.isFinished()) {
            break;
        }
    }
}

void VideoRender::DrawFrame(AVFrame* frame) {
    if (!frame || !frame->data[0]) return;

    __android_log_print(ANDROID_LOG_INFO, TAG, "VideoRender - 帧信息: 宽度 = %d, 高度 = %d, 格式 = %d, pts = %lld",
                        frame->width, frame->height, frame->format, (long long)frame->pts);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(program_);

    // 顶点坐标和纹理坐标
    static const GLfloat vertices[] = {
            -1.0f, -1.0f,  // 左下
            1.0f, -1.0f,   // 右下
            -1.0f, 1.0f,   // 左上
            1.0f, 1.0f     // 右上
    };

    static const GLfloat texCoords[] = {
            0.0f, 1.0f,    // 左下
            1.0f, 1.0f,    // 右下
            0.0f, 0.0f,    // 左上
            1.0f, 0.0f     // 右上
    };

    glVertexAttribPointer(positionHandle_, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(positionHandle_);

    glVertexAttribPointer(textureHandle_, 2, GL_FLOAT, GL_FALSE, 0, texCoords);
    glEnableVertexAttribArray(textureHandle_);

    // 上传纹理数据
    for (int i = 0; i < 3; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textureId_[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                     frame->width >> (i ? 1 : 0), frame->height >> (i ? 1 : 0),
                     0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                     frame->data[i]);
    }

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // 检查缓冲区交换是否成功
    if (surface_ == EGL_NO_SURFACE) {
        return;
    }
    __android_log_print(ANDROID_LOG_INFO, TAG, "Surface is valid");
    EGLBoolean result = eglSwapBuffers(display_, surface_);
    if (!result) {
        EGLint error = eglGetError();
        __android_log_print(ANDROID_LOG_ERROR, TAG, "eglSwapBuffers failed with error: %x", error);
    }
}

void VideoRender::Stop() {
    running_ = false;
}