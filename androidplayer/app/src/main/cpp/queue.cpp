#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <android/log.h>
#include "libavcodec/packet.h"
#include "libavutil/frame.h"

#define LOG_TAG "PacketQueue"
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))

template <typename T>
class PacketQueue {
public:
    void push(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(item);
        ++size_; // 入队时增加队列大小
        LOGI("队列大小增加: %zu", size_);
        cond_.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this]{ return !queue_.empty() || finished_; });

        if (queue_.empty() && finished_) {
            LOGI("队列为空且已标记为 finished");
            return nullptr;
        }

        T item = queue_.front();
        queue_.pop();
        --size_; // 出队时减少队列大小
        LOGI("队列大小减少: %zu", size_);
        return item;
    }

    void setFinished(bool finished) {
        std::lock_guard<std::mutex> lock(mutex_);
        finished_ = finished;
        LOGI("队列标记为 finished: %d", finished_);
        cond_.notify_all();
    }

    bool isFinished() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return finished_ && queue_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_; // 返回队列大小
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cond_;
    bool finished_ = false;
    size_t size_ = 0; // 记录队列大小
};

// 显式实例化模板类，支持 AVPacket 和 AVFrame
template class PacketQueue<AVPacket*>;
template class PacketQueue<AVFrame*>;