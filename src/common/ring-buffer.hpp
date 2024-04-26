#pragma once

// laar
#include <common/callback-queue.hpp>
#include <common/exceptions.hpp>

// std
#include <memory>
#include <mutex>


namespace laar {

    class RingBuffer {
    public:

        std::size_t writableSize();
        std::size_t readableSize();

        std::size_t write(const char* src, std::size_t size);
        std::size_t read(char* dest, std::size_t size);

        std::size_t peek(char* dest, std::size_t size);
        std::size_t drop(std::size_t size);

        char* rdCursor();
        char* wrCursor();

    private:
        // No private methods

    private:
        std::unique_ptr<std::vector<char>> buffer_;
        std::recursive_mutex lock_;
        std::size_t wPos_;
        std::size_t rPos_;
        bool empty_;

    };

}