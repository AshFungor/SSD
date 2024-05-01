#pragma once

// laar
#include <common/callback-queue.hpp>
#include <common/exceptions.hpp>

// std
#include <memory>
#include <mutex>


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

    class PlainBuffer : public IBuffer {
    public:

        PlainBuffer(std::size_t size);

        // IBuffer implementation
        virtual std::size_t writableSize() override;
        virtual std::size_t readableSize() override;
        virtual std::size_t write(const char* src, std::size_t size) override;
        virtual std::size_t read(char* dest, std::size_t size) override;
        virtual std::size_t peek(char* dest, std::size_t size) override;
        virtual std::size_t drop(std::size_t size) override;

        char* rPosition();
        char* wPosition();

        enum class EPosition {
            WPOS,
            RPOS
        };

        std::size_t advance(EPosition position, std::size_t size);
        void reset();

    private:
        // No private methods

    private:
        std::unique_ptr<std::vector<char>> buffer_;
        std::recursive_mutex lock_;
        std::size_t wPos_;
        std::size_t rPos_;

    };

}