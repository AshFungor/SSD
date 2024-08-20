// GTest
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>

// standard
#include <cmath>
#include <memory>
#include <climits>

// laar
#include <common/macros.hpp>
#include <common/exceptions.hpp>
#include <sounds/dispatchers/tube-dispatcher.hpp>
#include <sounds/dispatchers/bass-router-dispatcher.hpp>

// protos
#include <protos/client/simple/simple.pb.h>
#include <string>

using namespace NSound::NSimple;

#define GTEST_COUT(chain) \
    std::cerr << "[INFO      ] " << chain << '\n';

namespace {

    template<typename T>
    std::string arrayToString(T* array, std::size_t size) {
        std::string result;
        for (std::size_t i = 0; i < size; ++i) {
            if (i != 0) {
                result += " ";
            }
            result += std::to_string(array[i]);
        }
        return result;
    }

}

TEST(DispatcherTest, TestChannelIteratorInterleaved) {
    constexpr std::size_t samples = 20;
    constexpr std::size_t channels = 3;

    auto in = std::make_unique<std::int32_t[]>(samples * channels);
    auto wrapper = laar::StreamWrapper<std::int32_t>(
        samples, channels, laar::ESamplesOrder::INTERLEAVED, in.get()
    );

    for (std::size_t channel = 0; channel < channels; ++channel) {
        for (auto iter = wrapper.begin(channel); iter != wrapper.end(channel); ++iter) {
            *iter = channel;
        }
    }

    bool check = true;
    for (std::size_t sample = 0; sample < samples; ++sample) {
        for (std::size_t channel = 0; channel < channels; ++channel) {
            check &= in[sample * channels + channel] == channel;
        }
    }

    EXPECT_TRUE(check) 
        << "Expected interleaved samples pattern, received: "
        << arrayToString(in.get(), samples * channels);
}

TEST(DispatcherTest, TestChannelIteratorNoninterleaved) {
    constexpr std::size_t samples = 20;
    constexpr std::size_t channels = 3;

    auto in = std::make_unique<std::int32_t[]>(samples * channels);
    auto wrapper = laar::StreamWrapper<std::int32_t>(
        samples, channels, laar::ESamplesOrder::NONINTERLEAVED, in.get()
    );

    for (std::size_t channel = 0; channel < channels; ++channel) {
        for (auto iter = wrapper.begin(channel); iter != wrapper.end(channel); ++iter) {
            *iter = channel;
        }
    }

    bool check = true;
    for (std::size_t channel = 0; channel < channels; ++channel) {
        for (std::size_t sample = 0; sample < samples; ++sample) {
            check &= in[samples * channel + sample] == channel;
        }
    }

    EXPECT_TRUE(check) 
        << "Expected noninterleaved samples pattern, received: "
        << arrayToString(in.get(), samples * channels);
}

TEST(DispatcherTest, TestOneToMany) {
    constexpr std::size_t channels = 4;
    auto dispatcherOneToMany = laar::TubeDispatcher::create(
        laar::TubeDispatcher::EDispatchingDirection::ONE2MANY,
        laar::ESamplesOrder::NONINTERLEAVED,
        TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_32_LITTLE_ENDIAN,
        channels
    );

    // create arrays
    constexpr std::size_t samples = 20;
    auto in = std::make_unique<std::int32_t[]>(samples);
    auto out = std::make_unique<std::int32_t[]>(samples * channels);

    for (std::size_t i = 0; i < samples; ++i) {
        in[i] = INT32_MAX;
    }

    dispatcherOneToMany->dispatch(in.get(), out.get(), samples);

    auto wrapper = laar::StreamWrapper<std::int32_t>(
        samples, channels, laar::ESamplesOrder::NONINTERLEAVED, out.get()
    );

    bool check = true;
    for (std::size_t channel = 0; channel < channels; ++channel) {
        for (auto iter = wrapper.begin(channel); iter != wrapper.end(channel); ++iter) {
            check &= *iter == INT32_MAX;
        }
    }

    EXPECT_TRUE(check)
        << "expected copied stream to " << channels << "-channel output, got instead: "
        << arrayToString(out.get(), samples * channels);
}

TEST(DispatcherTest, TestManyToOne) {
    constexpr std::size_t channels = 4;
    auto dispatcherManyToOne = laar::TubeDispatcher::create(
        laar::TubeDispatcher::EDispatchingDirection::MANY2ONE,
        laar::ESamplesOrder::NONINTERLEAVED,
        TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_32_LITTLE_ENDIAN,
        channels
    );

    // create arrays
    constexpr std::size_t samples = 20;
    auto in = std::make_unique<std::int32_t[]>(samples * channels);
    auto out = std::make_unique<std::int32_t[]>(samples);

    for (std::size_t i = 0; i < samples * channels; ++i) {
        in[i] = INT32_MAX;
    }

    dispatcherManyToOne->dispatch(in.get(), out.get(), samples);

    auto wrapper = laar::StreamWrapper<std::int32_t>(
        samples, 1, laar::ESamplesOrder::NONINTERLEAVED, out.get()
    );

    bool check = true;
    for (auto iter = wrapper.begin(); iter != wrapper.end(); ++iter) {
        check &= *iter >= INT32_MAX - 2; // precision loss?
    }

    EXPECT_TRUE(check)
        << "expected copied stream to " << channels << "-channel output, got instead: "
        << arrayToString(out.get(), samples);
}

TEST(DispatcherTest, TestBassRouting) {
    auto bassRouteDispatcher = laar::BassRouterDispatcher::create(
        laar::ESamplesOrder::NONINTERLEAVED,
        TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_32_LITTLE_ENDIAN,
        44100,
        laar::BassRouterDispatcher::BassRange(20, 250),
        laar::BassRouterDispatcher::ChannelInfo(0, 1)
    );

    // create arrays
    constexpr std::size_t samples = 1000;
    auto in = std::make_unique<std::int32_t[]>(samples);
    auto out = std::make_unique<std::int32_t[]>(samples * 2);

    for (std::size_t i = 0; i < samples; ++i) {
        in[i] = std::sin(static_cast<std::int32_t>(i)) * INT32_MAX;
    }

    bassRouteDispatcher->dispatch(in.get(), out.get(), samples);

    auto wrapper = laar::StreamWrapper<std::int32_t>(
        samples, 2, laar::ESamplesOrder::NONINTERLEAVED, out.get()
    );

    bool check = false;

    EXPECT_TRUE(check)
        << "expected stream, got instead: "
        << arrayToString(out.get(), samples * 2);
}