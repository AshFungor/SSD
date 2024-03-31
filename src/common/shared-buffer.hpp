#pragma once

// standard
#include <cstddef>
#include <vector>


namespace laar {

    class SharedBuffer {
    public:

        SharedBuffer(std::size_t bufferSize);

        char* getRawWriteCursor(std::size_t writeSize);
        char* getRawReadCursor();

        void clear();
        void advanceReadCursor(std::size_t offset);

        std::size_t canProvide();
        std::size_t getCapacity();
        std::size_t getSize();

    private:
        std::vector<char> buffer_;
        std::size_t readCursor_;
        std::size_t writeCursor_;
        std::size_t size_;
    };
}