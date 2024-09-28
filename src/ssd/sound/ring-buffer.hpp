#pragma once

// std
#include <mutex>
#include <vector>
#include <memory>


namespace laar {

    class IBuffer {
    public:
        virtual std::size_t writableSize() = 0;
        virtual std::size_t readableSize() = 0;

        virtual std::size_t write(const char* src, std::size_t size) = 0;
        virtual std::size_t read(char* dest, std::size_t size) = 0;

        virtual std::size_t peek(char* dest, std::size_t size) = 0;
        virtual std::size_t drop(std::size_t size) = 0;

        virtual ~IBuffer() = default;
    };

    class RingBuffer : public IBuffer {
    public:

        RingBuffer(std::size_t size);

        // IBuffer implementation
        virtual std::size_t writableSize() override;
        virtual std::size_t readableSize() override;
        virtual std::size_t write(const char* src, std::size_t size) override;
        virtual std::size_t read(char* dest, std::size_t size) override;
        virtual std::size_t peek(char* dest, std::size_t size) override;
        virtual std::size_t drop(std::size_t size) override;

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