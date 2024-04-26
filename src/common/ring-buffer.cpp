// std
#include <cstring>
#include <memory>
#include <mutex>

#include "ring-buffer.hpp"

using namespace laar;


std::size_t RingBuffer::writableSize() {
    std::scoped_lock<std::recursive_mutex> locked(lock_);

    if (wPos_ > rPos_) {
        return wPos_ - wPos_;
    } else if (wPos_ < rPos_) {
        return buffer_->size() - rPos_ + wPos_;
    } else {
        // wpos == rpos
        return (empty_) ? buffer_->size() : 0;
    }
}

std::size_t RingBuffer::readableSize() {
    std::scoped_lock<std::recursive_mutex> locked(lock_);

     if (wPos_ > rPos_) {
        return buffer_->size() - wPos_ + rPos_;
    } else if (wPos_ < rPos_) {
        return rPos_ - wPos_;
    } else {
        // wpos == rpos
        return (empty_) ? buffer_->size() : 0;
    }
}

std::size_t RingBuffer::write(const char* src, std::size_t size) {
    std::scoped_lock<std::recursive_mutex> locked(lock_);

    std::size_t trailSize = buffer_->size() - wPos_;
    std::size_t maxSize = writableSize();

    if (size > maxSize) {
        size = maxSize;
    }

    if (size <= trailSize) {
        std::memcpy(buffer_->data() + wPos_, src, size);
        wPos_ += size;
    } else {
        std::memcpy(buffer_->data() + wPos_, src, trailSize);
        std::memcpy(buffer_->data(), src + trailSize, size - trailSize);
        wPos_ = size - trailSize;
    }

    if (size > 0) {
        empty_ = false;
    }

    return size;
}

std::size_t RingBuffer::read(char* dest, std::size_t size) {
    std::scoped_lock<std::recursive_mutex> locked(lock_);

    std::size_t trailSize = buffer_->size() - rPos_;
    std::size_t maxSize = readableSize();

    if (size > maxSize) {
        size = maxSize;
    }

    if (size <= trailSize) {
        std::memcpy(dest, buffer_->data() + rPos_, size);
        rPos_ += size;
    } else {
        std::memcpy(dest, buffer_->data() + rPos_, size);
        std::memcpy(dest + trailSize, buffer_->data(), size - trailSize);
        rPos_ = size - trailSize;
    }

    if (rPos_ == wPos_) {
        empty_ = true;
    }

    return size;
}

std::size_t RingBuffer::peek(char* dest, std::size_t size) {
    std::scoped_lock<std::recursive_mutex> locked(lock_);

    std::size_t trailSize = buffer_->size() - rPos_;
    std::size_t maxSize = readableSize();

    if (size > maxSize) {
        size = maxSize;
    }

    if (size <= trailSize) {
        std::memcpy(dest, buffer_->data() + rPos_, size);
    } else {
        std::memcpy(dest, buffer_->data() + rPos_, trailSize);
        std::memcpy(dest + trailSize, buffer_->data(), size - trailSize);
    }

    return size;
}

std::size_t RingBuffer::drop(std::size_t size) {
    std::scoped_lock<std::recursive_mutex> locked(lock_);

    std::size_t trailSize = buffer_->size() - rPos_;
    std::size_t maxSize = readableSize();

    if (size > maxSize) {
        size = maxSize;
    }

    if (size <= trailSize) {
        rPos_ += size;
    } else {
        rPos_ = size - trailSize;
    }

    if (rPos_ == wPos_) {
        empty_ = true;
    }

    return size;
}

char* RingBuffer::rdCursor() {
    return buffer_->data() + rPos_;
}

char* RingBuffer::wrCursor() {
    return buffer_->data() + wPos_;
}



