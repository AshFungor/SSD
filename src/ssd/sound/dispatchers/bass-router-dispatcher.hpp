#pragma once

// STD
#include <cstddef>
#include <cstdint>
#include <fftw3.h>
#include <memory>

// protos
#include <protos/client/simple/simple.pb.h>

// laar
#include <common/exceptions.hpp>
#include <sounds/interfaces/i-dispatcher.hpp>

// local
#include "tube-dispatcher.hpp"


namespace laar {

    class BassRouterDispatcher
        : IDispatcher
        , std::enable_shared_from_this<TubeDispatcher> {
    private: struct Private { };
    public:

        using TSampleFormat = NSound::NSimple::TSimpleMessage::TStreamConfiguration::TSampleSpecification::TFormat;

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
            const TSampleFormat& format,
            std::size_t sampleRate,
            BassRange range,
            ChannelInfo info
        );

        BassRouterDispatcher(
            const ESamplesOrder& order, 
            const TSampleFormat& format,
            std::size_t sampleRate,
            BassRange range,
            ChannelInfo info,
            Private access
        );

        // IDispatcher implementation
        WrappedResult dispatch(void* in, void* out, std::size_t samples) override;

    private:

        struct Frequencies {
            fftw_complex* bass;
            fftw_complex* normal;
        };

        Frequencies splitWindow(std::int32_t* in, std::size_t sampleRate, std::size_t samples);

        WrappedResult routeBass(void* in, void* out, std::size_t samples);
        WrappedResult routeNormal(void* in, void* out, std::size_t samples);

    private:
        const ESamplesOrder order_; 
        const TSampleFormat format_;
        const std::size_t sampleRate_;
        BassRange range_;
        ChannelInfo info_;

    };

}