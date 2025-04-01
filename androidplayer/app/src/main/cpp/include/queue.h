#ifndef PACKET_QUEUE_H
#define PACKET_QUEUE_H

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class PacketQueue {
public:
    void push(T item);
    T pop();
    void setFinished(bool finished);
    bool isFinished() const;
    size_t size() const; // 新增方法，用于获取队列大小

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cond_;
    bool finished_ = false;
    size_t size_ = 0; // 新增属性，记录队列大小
};

#endif