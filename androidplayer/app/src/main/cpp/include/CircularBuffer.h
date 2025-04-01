#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <vector>
#include <mutex>
#include <condition_variable>
extern "C" {
#include <libavutil/frame.h>
}

template <typename T>
class CircularBuffer {
public:
    explicit CircularBuffer(int capacity);
    void write(T item);
    T read();
    bool isEmpty() const;
    bool isFull() const;

private:
    int capacity_;
    int readIndex_;
    int writeIndex_;
    std::vector<T> buffer_;
    mutable std::mutex mutex_;
    std::condition_variable condNotEmpty_;
    std::condition_variable condNotFull_;
};

#endif