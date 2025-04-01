#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <mutex>
#include <condition_variable>

template <typename T>
class RingBuffer {
public:
    RingBuffer(size_t size) : buffer(size), capacity(size), read_pos(0), write_pos(0), count(0) {}

    ~RingBuffer() {}

    size_t write(const T* data, size_t size) {
        std::unique_lock<std::mutex> lock(mutex);
        // 等待缓冲区有足够的写入空间
        condNotFull.wait(lock, [this, size] { return available_write() >= size; });

        size_t to_write = size;
        if (to_write > 0) {
            size_t first_part = (capacity - write_pos < to_write) ? capacity - write_pos : to_write;
            std::memcpy(buffer.data() + write_pos, data, first_part * sizeof(T));
            std::memcpy(buffer.data(), data + first_part, (to_write - first_part) * sizeof(T));
            write_pos = (write_pos + to_write) % capacity;
            count += to_write;
        }
        // 通知可能正在等待数据的线程
        condNotEmpty.notify_one();
        return to_write;
    }

    size_t read(T* data, size_t size) {
        std::unique_lock<std::mutex> lock(mutex);
        // 等待缓冲区有足够的数据可读
        condNotEmpty.wait(lock, [this, size] { return available_read() >= size; });

        size_t to_read = size;
        if (to_read > 0) {
            size_t first_part = (capacity - read_pos < to_read) ? capacity - read_pos : to_read;
            std::memcpy(data, buffer.data() + read_pos, first_part * sizeof(T));
            std::memcpy(data + first_part, buffer.data(), (to_read - first_part) * sizeof(T));
            read_pos = (read_pos + to_read) % capacity;
            count -= to_read;
        }
        // 通知可能正在等待写入空间的线程
        condNotFull.notify_one();
        return to_read;
    }

    size_t available_read() const {
        return count;
    }

    size_t available_write() const {
        return capacity - count;
    }

    bool isEmpty() const {
        std::unique_lock<std::mutex> lock(mutex);
        return count == 0;
    }

    bool isFull() const {
        std::unique_lock<std::mutex> lock(mutex);
        return count == capacity;
    }

private:
    std::vector<T> buffer;
    size_t capacity;
    size_t read_pos;
    size_t write_pos;
    size_t count;
    mutable std::mutex mutex;
    std::condition_variable condNotFull;
    std::condition_variable condNotEmpty;
};

#endif // RINGBUFFER_H