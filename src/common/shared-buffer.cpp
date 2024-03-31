// standard
#include <cstddef>
#include <vector>

// local
#include "shared-buffer.hpp"

using namespace laar;


SharedBuffer::SharedBuffer(std::size_t bufferSize)
    : buffer_(bufferSize)
    , writeCursor_(0)
    , size_(0)
{}

char* SharedBuffer::getRawWriteCursor(std::size_t writeSize) {
    writeCursor_ += writeSize;
    return buffer_.data() + writeCursor_;
}

char* SharedBuffer::getRawReadCursor() {
    return buffer_.data();
}

void SharedBuffer::clear() {
    writeCursor_ = readCursor_ = size_ = 0;
}

std::size_t SharedBuffer::getCapacity() {
    return buffer_.size();
}

std::size_t SharedBuffer::getSize() {
    return size_;
}

std::size_t SharedBuffer::canProvide() {
    return buffer_.size() - size_;
}

void SharedBuffer::advanceReadCursor(std::size_t offset) {
    readCursor_ += offset;
}