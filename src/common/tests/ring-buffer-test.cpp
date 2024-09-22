// GTest
#include <gtest/gtest.h>

// standard
#include <cstring>

// laar
#include <src/common/ring-buffer.hpp>

TEST(RingBufferTest, IOOperationsTest) {
    std::size_t bufferSize = 20;
    laar::RingBuffer buffer (bufferSize);

    char str[14] = "hello, world!";
    auto written = buffer.write(str, sizeof(str));

    EXPECT_EQ(written, sizeof(str));
    EXPECT_EQ(buffer.writableSize(), bufferSize - sizeof(str));
    EXPECT_EQ(buffer.readableSize(), sizeof(str));

    char dest[sizeof(str)];
    buffer.read(dest, sizeof(str));

    EXPECT_EQ(buffer.writableSize(), bufferSize);
    EXPECT_EQ(buffer.readableSize(), 0);

    EXPECT_EQ(std::memcmp(dest, str, sizeof(str)), 0);

    // check how buffer wraps
    char anotherStr[10] = "Lol kek??";
    buffer.write(anotherStr, sizeof(anotherStr));
    buffer.write(anotherStr, sizeof(anotherStr));

    EXPECT_EQ(buffer.writableSize(), 0);
    EXPECT_EQ(buffer.readableSize(), bufferSize);

    buffer.peek(dest, sizeof(anotherStr));
    EXPECT_EQ(std::memcmp(dest, anotherStr, sizeof(anotherStr)), 0);

    EXPECT_EQ(buffer.writableSize(), 0);
    EXPECT_EQ(buffer.readableSize(), bufferSize);

    buffer.drop(bufferSize);

    EXPECT_EQ(buffer.writableSize(), bufferSize);
    EXPECT_EQ(buffer.readableSize(), 0);
}