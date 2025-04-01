#ifndef VIDEO_DECODER_H
#define VIDEO_DECODER_H

#include <android/native_window.h>
#include "context.h"
#include "queue.h"
extern "C" {
#include <libswscale/swscale.h>
}
class VideoDecoder {
public:
    explicit VideoDecoder(VideoProcessingContext& ctx);
    bool setupDecoder();
    void decode(PacketQueue<AVPacket*>& packetQueue,PacketQueue<AVFrame*>& frameQueue,ANativeWindow* window);
private:
    VideoProcessingContext& ctx_;
    SwsContext* sws_ctx_ = nullptr;
};

#endif