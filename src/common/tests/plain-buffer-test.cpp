// GTest
#include <gtest/gtest.h>

// standard
#include <cstring>

// laar
#include <src/common/plain-buffer.hpp>

TEST(PlainBufferTest, IOOperationsTest) {
    std::size_t bufferSize = 20;
    laar::PlainBuffer buffer (bufferSize);

    char str[14] = "hello, world!";
    auto written = buffer.write(str, sizeof(str));

    EXPECT_EQ(written, sizeof(str));
    EXPECT_EQ(buffer.writableSize(), bufferSize - sizeof(str));
    EXPECT_EQ(buffer.readableSize(), sizeof(str));

    char dest[sizeof(str)];
    buffer.read(dest, sizeof(str));

    EXPECT_EQ(buffer.writableSize(), bufferSize - sizeof(str));
    EXPECT_EQ(buffer.readableSize(), 0);

    EXPECT_EQ(std::memcmp(dest, str, sizeof(str)), 0);

    buffer.write(str, sizeof(str));
    buffer.reset();

    EXPECT_EQ(buffer.writableSize(), bufferSize);
    EXPECT_EQ(buffer.readableSize(), 0);

    EXPECT_EQ(std::memcmp(str, buffer.rPosition(), sizeof(str)), 0);

    buffer.advance(laar::PlainBuffer::EPosition::WPOS, buffer.writableSize());
    buffer.advance(laar::PlainBuffer::EPosition::RPOS, sizeof(str));

    EXPECT_EQ(buffer.writableSize(), 0);
    EXPECT_EQ(buffer.readableSize(), bufferSize - sizeof(str));
}