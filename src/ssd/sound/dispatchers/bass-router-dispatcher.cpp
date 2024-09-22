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
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>
#include <src/ssd/sound/dispatchers/tube-dispatcher.hpp>
#include <src/ssd/sound/dispatchers/bass-router-dispatcher.hpp>

using namespace laar;
using ESamples = NSound::NClient::NBase::TBaseMessage::TStreamConfiguration::TSampleSpecification;


namespace {

    struct RoutingChannelInfo : BassRouterDispatcher::ChannelInfo {
        RoutingChannelInfo(std::size_t routeTo, std::size_t leaveSilent, BassRouterDispatcher::ChannelInfo info)
            : BassRouterDispatcher::ChannelInfo(info)
            , routeTo(routeTo)
            , leaveSilent(leaveSilent)
        {}

        std::size_t routeTo;
        std::size_t leaveSilent;
    };

    struct TubeState {
        TubeState(const ESamplesOrder& order)
            : order_(order)
        {}

        const ESamplesOrder order_;
    };

    template<typename ResultingSampleType, typename FunctorType>
    absl::Status typedRoute(void* in, void* out, std::size_t samples, TubeState state, RoutingChannelInfo routingInfo, FunctorType process) {
        fftw_complex* original = static_cast<fftw_complex*>(in);

        ResultingSampleType* resulting = static_cast<ResultingSampleType*>(out);
        StreamWrapper<ResultingSampleType> outWrappedStream(
            samples, routingInfo.channelNum, state.order_, resulting 
        );

        auto outCurrent = outWrappedStream.begin(routingInfo.routeTo);
        for (std::size_t sample = 0; sample < samples; ++sample) {
            if (outCurrent == outWrappedStream.end(routingInfo.routeTo)) {
                return absl::OutOfRangeError("reached end of out stream unexpectedly");
            }
            *outCurrent = process(static_cast<std::int32_t>(original[sample][0] / samples));
            ++outCurrent;
        }
        
        return absl::OkStatus();
    }

}


std::shared_ptr<BassRouterDispatcher> BassRouterDispatcher::create(
    const ESamplesOrder& order, 
    const ESampleType& format,
    std::size_t sampleRate,
    BassRange range,
    ChannelInfo info
) {
    return std::make_shared<BassRouterDispatcher>(order, format, sampleRate, range, info, Private());
}

BassRouterDispatcher::BassRouterDispatcher(
    const ESamplesOrder& order, 
    const ESampleType& format,
    std::size_t sampleRate,
    BassRange range,
    ChannelInfo info,
    Private access
) 
    : order_(order)
    , format_(format)
    , sampleRate_(sampleRate)
    , range_(range)
    , info_(info)
{}

BassRouterDispatcher::Frequencies BassRouterDispatcher::splitWindow(std::int32_t* in, std::size_t sampleRate, std::size_t samples) {
    fftw_complex *complexIn = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * samples);
    fftw_complex *complexOut = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * samples);

    for (std::size_t i = 0; i < samples; ++i) {
        complexIn[i][0] = in[i];
        complexIn[i][1] = 0.0;
    }

    fftw_plan p = fftw_plan_dft_1d(samples, complexIn, complexOut, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(p);

    double resolution = static_cast<double>(sampleRate) / samples;
    Frequencies frequencies;
    frequencies.bass = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * samples);
    frequencies.normal = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * samples);

    bool bassEmpty = true;
    bool normalEmpty = true;
    for (int i = 0; i < samples / 2; ++i) {
        double freq = i * resolution;

        if (freq >= range_.lower && freq <= range_.higher) {
            frequencies.bass[i][0] = complexOut[i][0];
            frequencies.bass[i][1] = complexOut[i][1];
            frequencies.normal[i][0] = 0;
            frequencies.normal[i][1] = 0;
            bassEmpty = false; 
        } else {
            frequencies.bass[i][0] = 0;
            frequencies.bass[i][1] = 0;
            frequencies.normal[i][0] = complexOut[i][0];
            frequencies.normal[i][1] = complexOut[i][1];
            normalEmpty = false;
        }
    }
    if (!normalEmpty) {
        fftw_plan normalBackwardPlan = fftw_plan_dft_1d(samples, frequencies.normal, complexIn, FFTW_BACKWARD, FFTW_ESTIMATE);
        fftw_execute(normalBackwardPlan);
        fftw_destroy_plan(normalBackwardPlan);
    } else {
        for (int i = 0; i < samples; ++i) {
            complexIn[i][0] = laar::Silence * samples;
            complexIn[i][1] = 0;
        }
    }

    if (!bassEmpty) {
        fftw_plan bassBackwardPlan = fftw_plan_dft_1d(samples, frequencies.bass, complexOut, FFTW_BACKWARD, FFTW_ESTIMATE);
        fftw_execute(bassBackwardPlan);
        fftw_destroy_plan(bassBackwardPlan);
    } else {
        for (int i = 0; i < samples; ++i) {
            complexOut[i][0] = laar::Silence * samples;
            complexOut[i][1] = 0;
        }
    }

    fftw_free(frequencies.normal);
    fftw_free(frequencies.bass);

    fftw_destroy_plan(p);

    frequencies.normal = complexIn;
    frequencies.bass = complexOut;

    return frequencies;
}

absl::Status BassRouterDispatcher::dispatch(void* in, void* out, std::size_t samples) {
    auto data = splitWindow(reinterpret_cast<std::int32_t*>(in), sampleRate_, samples);

    absl::Status result = absl::OkStatus();
    result.Update(routeBass(data.bass, out, samples));
    result.Update(routeNormal(data.normal, out, samples));
    fftw_free(data.normal);
    fftw_free(data.bass);
    return result;
}

absl::Status BassRouterDispatcher::routeBass(void* in, void* out, std::size_t samples){
    TubeState state (order_);
    RoutingChannelInfo routingInfo (info_.bass, info_.normal, info_);

    switch (format_) {
        case ESamples::FLOAT_32_BIG_ENDIAN:
            return typedRoute<std::uint32_t>(in, out, samples, state, routingInfo, &convertToFloat32BE);
        case ESamples::FLOAT_32_LITTLE_ENDIAN:
            return typedRoute<std::uint32_t>(in, out, samples, state, routingInfo, &convertToFloat32LE);
        case ESamples::UNSIGNED_8:
            return typedRoute<std::uint8_t>(in, out, samples, state, routingInfo, &convertToUnsigned8);
        case ESamples::SIGNED_32_BIG_ENDIAN:
            return typedRoute<std::uint32_t>(in, out, samples, state, routingInfo, &convertToSigned32BE);
        case ESamples::SIGNED_32_LITTLE_ENDIAN:
            return typedRoute<std::uint32_t>(in, out, samples, state, routingInfo, &convertToSigned32LE);
        case ESamples::SIGNED_16_BIG_ENDIAN:
            return typedRoute<std::uint16_t>(in, out, samples, state, routingInfo, &convertToSigned16BE);
        case ESamples::SIGNED_16_LITTLE_ENDIAN:
            return typedRoute<std::uint16_t>(in, out, samples, state, routingInfo, &convertToSigned16LE);
        default:
            return absl::InvalidArgumentError("format is not supported");
    }
}

absl::Status BassRouterDispatcher::routeNormal(void* in, void* out, std::size_t samples){
    TubeState state (order_);
    RoutingChannelInfo routingInfo (info_.normal, info_.bass, info_);

    switch (format_) {
        case ESamples::FLOAT_32_BIG_ENDIAN:
            return typedRoute<std::uint32_t>(in, out, samples, state, routingInfo, &convertToFloat32BE);
        case ESamples::FLOAT_32_LITTLE_ENDIAN:
            return typedRoute<std::uint32_t>(in, out, samples, state, routingInfo, &convertToFloat32LE);
        case ESamples::UNSIGNED_8:
            return typedRoute<std::uint8_t>(in, out, samples, state, routingInfo, &convertToUnsigned8);
        case ESamples::SIGNED_32_BIG_ENDIAN:
            return typedRoute<std::uint32_t>(in, out, samples, state, routingInfo, &convertToSigned32BE);
        case ESamples::SIGNED_32_LITTLE_ENDIAN:
            return typedRoute<std::uint32_t>(in, out, samples, state, routingInfo, &convertToSigned32LE);
        case ESamples::SIGNED_16_BIG_ENDIAN:
            return typedRoute<std::uint16_t>(in, out, samples, state, routingInfo, &convertToSigned16BE);
        case ESamples::SIGNED_16_LITTLE_ENDIAN:
            return typedRoute<std::uint16_t>(in, out, samples, state, routingInfo, &convertToSigned16LE);
        default:
            return absl::InvalidArgumentError("format is not supported");
    }
}

BassRouterDispatcher::BassRange::BassRange(double lower, double higher) 
    : lower(lower)
    , higher(higher)
{}

BassRouterDispatcher::ChannelInfo::ChannelInfo(std::size_t normal, std::size_t bass) 
    : normal(normal)
    , bass(bass)
{}