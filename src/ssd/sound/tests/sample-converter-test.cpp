// GTest
#include <gtest/gtest.h>

// standard
#include <cmath>
#include <cstdint>
#include <iostream>
#include <type_traits>

// laar
#include <src/ssd/sound/converter.hpp>

#define GTEST_COUT(chain) \
    std::cerr << "[INFO      ] " << chain << '\n';

namespace {

    constexpr int steps = 20;

    template<typename IntegerHolder, typename IterType>
    std::vector<IntegerHolder> makeRange(IntegerHolder start, IntegerHolder end, IntegerHolder step) {
        std::vector<IntegerHolder> values;
        values.reserve((end - start) / step);
        GTEST_COUT("generating values with start: " << start << ", end: " << end << " step: " << step);
        for (IterType i = start; i < end; i += step) {
            GTEST_COUT("value: " << i);
            values.push_back(i);
        }
        return values;
    }

    template<typename SampleHolder>
    class ConvertWrapper {
    public:
        ConvertWrapper(const std::function<std::int32_t(SampleHolder)>& converter)
        : converter_(std::move(converter))
        {}

        std::int32_t operator()(SampleHolder sample) const {
            return converter_(sample);
        }

    private:
        const std::function<std::int32_t(SampleHolder)> converter_;
    };

    class SampleTest : public testing::Test {
    public:
        void SetUp() override {}
        void TearDown() override {}
        // testing body
        void testSampling() {

        }

        template<typename SampleHolder, typename SampleType, typename RangeGeneratorType>
        void testResampling(SampleType min, SampleType max, ConvertWrapper<SampleHolder> convert) {

            std::vector<SampleType> values;
            if (std::is_floating_point<SampleType>::value) {
                GTEST_COUT("step for converted values: " << ((long double) max - min) / steps);
                values = makeRange<SampleType, RangeGeneratorType>(min, max, ((long double) max - min) / steps);
            } else {
                GTEST_COUT("step for converted values: " << std::ceil(((long double) max - min) / steps));
                values = makeRange<SampleType, RangeGeneratorType>(min, max, std::ceil(((long double) max - min) / steps));
            }

            // test how max/min sample values translate into base sample type min and max
            // test min = base min
            // test max = base max
            GTEST_COUT("casted values to holder type: min: " << reinterpret_cast<SampleHolder&>(min) 
                << "; max: " << reinterpret_cast<SampleHolder&>(max));
            EXPECT_EQ(convert(reinterpret_cast<SampleHolder&>(min)), INT32_MIN);
            // drop last digit because precision is for fuckers
            EXPECT_EQ(convert(reinterpret_cast<SampleHolder&>(max)) / 10, INT32_MAX / 10);
            
            auto step = std::ceil(((long double) INT32_MAX - INT32_MIN) / steps);
            GTEST_COUT("step for actual values: " << step);
            auto actual = makeRange<std::int32_t, std::int64_t>(INT32_MIN, INT32_MAX, step);

            ASSERT_EQ(actual.size(), values.size());
            std::int32_t prevDist;
            bool isFirst = true;

            // first process negative
            for (std::size_t i = 1; i < actual.size() / 2; ++i) {
                auto currActual = actual[i];
                auto currConverted = convert(reinterpret_cast<SampleHolder&>(values[i]));
                auto prevConverted = convert(reinterpret_cast<SampleHolder&>(values[i - 1]));
                GTEST_COUT("Value on iteration " << i << " actual: " << currActual << " converted: " << currConverted << " from: " << (int) values[i]);
                auto dist = std::abs(currConverted - prevConverted);
                
                if (isFirst) {
                    isFirst = false;
                    prevDist = dist;
                } else {
                    if (std::is_floating_point<SampleType>::value) {
                        // float arithmetics...
                        EXPECT_EQ(dist / 1000, prevDist / 1000);
                    } else {
                        EXPECT_EQ(dist, prevDist);
                    }
                }
            }

            // then positive
            isFirst = true;
            for (std::size_t i = actual.size() / 2 + 1; i < actual.size(); ++i) {
                auto currActual = actual[i];
                auto currConverted = convert(reinterpret_cast<SampleHolder&>(values[i]));
                auto prevConverted = convert(reinterpret_cast<SampleHolder&>(values[i - 1]));
                GTEST_COUT("Value on iteration " << i << " actual: " << currActual << " converted: " << currConverted << " from: " << (int) values[i]);
                auto dist = std::abs(currConverted - prevConverted);
                
                if (isFirst) {
                    isFirst = false;
                    prevDist = dist;
                } else {
                    if (std::is_floating_point<SampleType>::value) {
                        EXPECT_EQ(dist / 1000, prevDist / 1000);
                    } else {
                        EXPECT_EQ(dist, prevDist);
                    }
                }
            }
        }

    };
}

TEST(SoundTest, EndiannessTest) {
    constexpr std::uint32_t var32LE = 0x00FF0FF0;
    constexpr std::uint32_t var32BE = 0xF00FFF00;
    constexpr std::uint16_t var16LE = 0x0FF0;
    constexpr std::uint16_t var16BE = 0xF00F;

    std::uint32_t variable32;
    std::uint16_t variable16;
    #if __BYTE_ORDER == __LITTLE_ENDIAN
        variable32 = var32LE;
        EXPECT_EQ(laar::convertToBE(variable32), var32BE)
            << "values are not equal: " << std::hex << 
            laar::convertToBE(variable32) << " and " << var32BE
            << std::dec;
        EXPECT_EQ(laar::convertFromBE(laar::convertToBE(variable32)), var32LE)
            << "values are not equal: " << std::hex << 
            laar::convertFromBE(laar::convertToBE(variable32)) << " and " << var32BE
            << std::dec;
        variable16 = var16LE;
        EXPECT_EQ(laar::convertToBE(variable16), var16BE)
            << "values are not equal: " << std::hex << 
            laar::convertToBE(variable16) << " and " << var16BE
            << std::dec;
        EXPECT_EQ(laar::convertFromBE(laar::convertToBE(variable16)), var16LE)
            << "values are not equal: " << std::hex << 
            laar::convertFromLE(laar::convertToBE(variable16)) << " and " << var16BE
            << std::dec;
    #elif __BYTE_ORDER == __BIG_ENDIAN
        variable32 = var32BE;
        EXPECT_EQ(laar::convertToLE(variable32), var32LE);
        EXPECT_EQ(laar::convertFromLE(laar::convertToLE(variable32)), var32BE);
        variable16 = var16BE;
        EXPECT_EQ(laar::convertToBE(variable16), var32LE);
        EXPECT_EQ(laar::convertFromLE(laar::convertToLE(variable16)), var32BE);
    #else
        #error Endianess is unsupported
    #endif 
}

TEST_F(SampleTest, ResamplingTestU8) {
    testResampling<std::uint8_t, std::uint8_t, std::int64_t>(0, UINT8_MAX, ConvertWrapper<std::uint8_t>{&laar::convertFromUnsigned8});
}

TEST_F(SampleTest, ResamplingTestS16) {
    std::int16_t max = INT16_MAX;
    std::int16_t min = INT16_MIN;
    testResampling<std::uint16_t, std::int16_t, std::int64_t>(
        min, 
        max, 
        ConvertWrapper<std::uint16_t>{&laar::convertFromSigned16LE}
    );
    // fuck BE
}

TEST_F(SampleTest, ResamplingTestS32) {
    std::int32_t max = INT32_MAX;
    std::int32_t min = INT32_MIN;
    testResampling<std::uint32_t, std::int32_t, std::int64_t>(
        min, 
        max, 
        ConvertWrapper<std::uint32_t>{&laar::convertFromSigned32LE}
    );
}

TEST_F(SampleTest, ResamplingTestF32) {
    float max = 1.0f;
    float min = -1.0f;
    testResampling<std::uint32_t, float, long double>(
        min, 
        max, 
        ConvertWrapper<std::uint32_t>{&laar::convertFromFloat32LE}
    );
}