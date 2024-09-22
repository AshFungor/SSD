#pragma once

// abseil
#include <absl/status/status.h>

// fftw3
#include <fftw3.h>

// STD
#include <memory>
#include <cstddef>
#include <cstdint>

// laar
#include <src/common/exceptions.hpp>
#include <src/ssd/sound/converter.hpp>
#include <src/ssd/sound/interfaces/i-dispatcher.hpp>
#include <src/ssd/sound/dispatchers/tube-dispatcher.hpp>


namespace laar {

    class BassRouterDispatcher
        : IDispatcher
        , std::enable_shared_from_this<TubeDispatcher> {
    private: struct Private { };
    public:

        struct BassRange {
            BassRange(double lower, double higher);

            double lower;
            double higher;
        };

        struct ChannelInfo {
            ChannelInfo(std::size_t normal, std::size_t bass);

            const std::size_t channelNum = 2;
            const std::size_t normal;
            const std::size_t bass; 
        };

        static std::shared_ptr<BassRouterDispatcher> create(
            const ESamplesOrder& order, 
            const ESampleType& format,
            std::size_t sampleRate,
            BassRange range,
            ChannelInfo info
        );

        BassRouterDispatcher(
            const ESamplesOrder& order, 
            const ESampleType& format,
            std::size_t sampleRate,
            BassRange range,
            ChannelInfo info,
            Private access
        );

        // IDispatcher implementation
        absl::Status dispatch(void* in, void* out, std::size_t samples) override;

    private:

        struct Frequencies {
            fftw_complex* bass;
            fftw_complex* normal;
        };

        Frequencies splitWindow(std::int32_t* in, std::size_t sampleRate, std::size_t samples);

        absl::Status routeBass(void* in, void* out, std::size_t samples);
        absl::Status routeNormal(void* in, void* out, std::size_t samples);

    private:
        const ESamplesOrder order_; 
        const ESampleType format_;
        const std::size_t sampleRate_;
        const BassRange range_;
        const ChannelInfo info_;

    };

}