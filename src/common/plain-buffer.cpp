// std
#include <cstring>
#include <memory>
#include <mutex>

#include "plain-buffer.hpp"

using namespace laar;


PlainBuffer::PlainBuffer(std::size_t size) 
: buffer_(std::make_unique<std::vector<char>>(size))
, wPos_(0)
, rPos_(0)
{}

std::size_t PlainBuffer::writableSize() {
    std::scoped_lock<std::recursive_mutex> locked(lock_);

    return buffer_->size() - wPos_; 
}

std::size_t PlainBuffer::readableSize() {
    std::scoped_lock<std::recursive_mutex> locked(lock_);

    return wPos_ - rPos_;
}

std::size_t PlainBuffer::write(const char* src, std::size_t size) {
    std::scoped_lock<std::recursive_mutex> locked(lock_);

    std::size_t maxSize = writableSize();

    if (size > maxSize) {
        size = maxSize;
    }

    std::memcpy(buffer_->data() + wPos_, src, size);
    wPos_ += size;

    return size;
}

std::size_t PlainBuffer::read(char* dest, std::size_t size) {
    std::scoped_lock<std::recursive_mutex> locked(lock_);

    std::size_t maxSize = readableSize();

    if (size > maxSize) {
        size = maxSize;
    }

    std::memcpy(dest, buffer_->data() + rPos_, size);
    rPos_ += size;

    return size;
}

std::size_t PlainBuffer::peek(char* dest, std::size_t size) {
    std::scoped_lock<std::recursive_mutex> locked(lock_);

    std::size_t maxSize = readableSize();

    if (size > maxSize) {
        size = maxSize;
    }

    std::memcpy(dest, buffer_->data() + rPos_, size);
    rPos_ += size;

    return size;
}

std::size_t PlainBuffer::drop(std::size_t size) {
    return advance(EPosition::RPOS, size);
}

char* PlainBuffer::rPosition() {
    // FIXME: Segfault if locking mutex?
    // std::scoped_lock<std::recursive_mutex> locked(lock_);
    return buffer_->data() + rPos_;
}

char* PlainBuffer::wPosition() {
    // FIXME: Segfault if locking mutex?
    // std::scoped_lock<std::recursive_mutex> locked(lock_);
    return buffer_->data() + wPos_;
}

std::size_t PlainBuffer::advance(EPosition position, std::size_t size) {
    std::scoped_lock<std::recursive_mutex> locked(lock_);

    std::size_t newPos = 0;
    std::size_t distance = 0;

    switch (position) {
        case EPosition::RPOS:
            newPos = std::min(wPos_, rPos_ + size);
            distance = newPos - rPos_;
            rPos_ = newPos;
            return distance;
        case EPosition::WPOS:
            newPos = std::min(buffer_->size(), wPos_ + size);
            newPos = newPos - wPos_;
            wPos_ = newPos;
            return distance;
    }

    return 0;
}

void PlainBuffer::reset() {
    std::scoped_lock<std::recursive_mutex> locked(lock_);
    wPos_ = rPos_ = 0;
}
