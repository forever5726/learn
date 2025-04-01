#ifndef DEMUXER_H
#define DEMUXER_H

#include "context.h"
#include "queue.h"
#include "audioContext.h"

class Demuxer {
public:
    explicit Demuxer(VideoProcessingContext& ctx,AudioProcessingContext& audioctx);
    bool openInput(const char* url);
    void start(PacketQueue<AVPacket*>& packetQueue);

    // 新增方法声明
    bool openInputWithAudio(const char* url);
    void startWithAudio(PacketQueue<AVPacket*>& videoPacketQueue, PacketQueue<AVPacket*>& audioPacketQueue);

private:
    VideoProcessingContext& ctx_;
    AudioProcessingContext& audio_ctx_;
};

#endif