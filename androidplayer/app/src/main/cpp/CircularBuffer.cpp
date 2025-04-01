#include "CircularBuffer.h"

template <typename T>
CircularBuffer<T>::CircularBuffer(int capacity) : capacity_(capacity), readIndex_(0), writeIndex_(0) {
    buffer_.resize(capacity);
}

template <typename T>
void CircularBuffer<T>::write(T item) {
    std::unique_lock<std::mutex> lock(mutex_);
    // 等待缓冲器有空间
    while (isFull()) {
        condNotFull_.wait(lock);
    }

    buffer_[writeIndex_] = item;
    writeIndex_ = (writeIndex_ + 1) % capacity_;
    condNotEmpty_.notify_one();
}

template <typename T>
T CircularBuffer<T>::read() {
    std::unique_lock<std::mutex> lock(mutex_);
    // 等待缓冲器有数据
    while (isEmpty()) {
        condNotEmpty_.wait(lock);
    }

    T item = buffer_[readIndex_];
    buffer_[readIndex_] = nullptr;
    readIndex_ = (readIndex_ + 1) % capacity_;
    condNotFull_.notify_one();
    return item;
}

template <typename T>
bool CircularBuffer<T>::isEmpty() const {
    return readIndex_ == writeIndex_;
}

template <typename T>
bool CircularBuffer<T>::isFull() const {
    return (writeIndex_ + 1) % capacity_ == readIndex_;
}

// 显式实例化模板类，用于处理 AVFrame* 类型
template class CircularBuffer<AVFrame*>;