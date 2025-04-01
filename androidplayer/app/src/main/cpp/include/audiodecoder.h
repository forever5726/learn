#ifndef AUDIO_DECODER_H
#define AUDIO_DECODER_H

#include "audioContext.h"  // 引入音频处理上下文头文件
#include "RingBuffer.h"  // 引入环形缓冲区头文件
#include "queue.h"  // 添加包含 PacketQueue 定义的头文件

extern "C" {
#include <libswresample/swresample.h>
}

class AudioDecoder {
public:
    explicit AudioDecoder(AudioProcessingContext& ctx);
    bool setupDecoder();
    void decode(PacketQueue<AVPacket*>& packetQueue, RingBuffer<uint8_t>& ringBuffer);
private:
    AudioProcessingContext& ctx_;
    SwrContext* swr_ctx_ = nullptr;
};

#endif