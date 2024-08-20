// STD
#include <cstddef>
#include <cstdint>
#include <memory>

// protos
#include <protos/client/simple/simple.pb.h>

// laar
#include <common/exceptions.hpp>
#include <sounds/interfaces/i-dispatcher.hpp>
#include <sounds/interfaces/i-audio-handler.hpp>

// fft
#include <fftw3.h>

// local
#include "tube-dispatcher.hpp"
#include "bass-router-dispatcher.hpp"

using namespace laar;
using namespace NSound::NSimple;

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
    WrappedResult typedRoute(void* in, void* out, std::size_t samples, TubeState state, RoutingChannelInfo routingInfo, FunctorType process) {
        std::int32_t* original = static_cast<std::int32_t*>(in);
        StreamWrapper<std::int32_t> inWrappedStream(
            samples, 1, state.order_, original 
        );

        ResultingSampleType* resulting = static_cast<ResultingSampleType*>(out);
        StreamWrapper<ResultingSampleType> outWrappedStream(
            samples, routingInfo.channelNum, state.order_, resulting 
        );

        for (auto outCurrent = outWrappedStream.begin(routingInfo.leaveSilent); outCurrent != outWrappedStream.end(routingInfo.leaveSilent); ++outCurrent) {
            *outCurrent = process(Silence);
        }

        auto outCurrent = outWrappedStream.begin(routingInfo.routeTo);
        for (auto inCurrent = inWrappedStream.begin(); inCurrent != inWrappedStream.end(); ++inCurrent) {
            *outCurrent = process(*inCurrent);
            if (++outCurrent; outCurrent == outWrappedStream.end(routingInfo.routeTo)) {
                return WrappedResult::wrapError("reached end of out stream unexpectedly");
            }
        }
        
        return WrappedResult::wrapResult();
    }

}


std::shared_ptr<BassRouterDispatcher> BassRouterDispatcher::create(
    const ESamplesOrder& order, 
    const TSampleFormat& format,
    std::size_t sampleRate,
    BassRange range,
    ChannelInfo info
) {
    return std::make_shared<BassRouterDispatcher>(order, format, sampleRate, range, info, Private());
}

BassRouterDispatcher::BassRouterDispatcher(
    const ESamplesOrder& order, 
    const TSampleFormat& format,
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

bool BassRouterDispatcher::validateWindow(std::int32_t* in, std::size_t sampleRate, std::size_t samples) {
    fftw_complex *complexIn = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * samples);
    fftw_complex *complexOut = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * samples);

    for (std::size_t i = 0; i < samples; ++i) {
        complexIn[i][0] = in[i];
        complexIn[i][1] = 0.0;
    }

    fftw_plan p = fftw_plan_dft_1d(samples, complexIn, complexOut, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(p);

    double resolution = static_cast<double>(sampleRate) / samples;

    double maxBassAmplitude = 0.0;
    double maxAmplitude = 0.0; 

    for (int i = 0; i < samples / 2; ++i) {
        double freq = i * resolution;
        double amplitude = std::sqrt(complexOut[i][0] * complexOut[i][0] + complexOut[i][1] * complexOut[i][1]);

        if (freq >= range_.lower && freq <= range_.higher) {
            maxBassAmplitude = std::max(maxBassAmplitude, amplitude);
        }
        maxAmplitude = std::max(maxAmplitude, amplitude);
    }

    double threshold = 0.5 * maxAmplitude;
    // std::cerr << (std::size_t) maxBassAmplitude << " " << (std::size_t) maxAmplitude << "\n";

    fftw_destroy_plan(p);
    fftw_free(complexIn);
    fftw_free(complexOut);

    return maxBassAmplitude > threshold;
}

WrappedResult BassRouterDispatcher::dispatch(void* in, void* out, std::size_t samples) {
    if (validateWindow(reinterpret_cast<std::int32_t*>(in), sampleRate_, samples)) {
        return routeBass(in, out, samples);
    }
    return routeNormal(in, out, samples);
}

WrappedResult BassRouterDispatcher::routeBass(void* in, void* out, std::size_t samples){
    TubeState state (order_);
    RoutingChannelInfo routingInfo (info_.bass, info_.normal, info_);

    switch (format_) {
        case TSimpleMessage::TStreamConfiguration::TSampleSpecification::FLOAT_32_BIG_ENDIAN:
            return typedRoute<std::uint32_t>(in, out, samples, state, routingInfo, &convertToFloat32BE);
        case TSimpleMessage::TStreamConfiguration::TSampleSpecification::FLOAT_32_LITTLE_ENDIAN:
            return typedRoute<std::uint32_t>(in, out, samples, state, routingInfo, &convertToFloat32LE);
        case TSimpleMessage::TStreamConfiguration::TSampleSpecification::UNSIGNED_8:
            return typedRoute<std::uint8_t>(in, out, samples, state, routingInfo, &convertToUnsigned8);
        case TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_32_BIG_ENDIAN:
            return typedRoute<std::uint32_t>(in, out, samples, state, routingInfo, &convertToSigned32BE);
        case TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_32_LITTLE_ENDIAN:
            return typedRoute<std::uint32_t>(in, out, samples, state, routingInfo, &convertToSigned32LE);
        case TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_16_BIG_ENDIAN:
            return typedRoute<std::uint16_t>(in, out, samples, state, routingInfo, &convertToSigned16BE);
        case TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_16_LITTLE_ENDIAN:
            return typedRoute<std::uint16_t>(in, out, samples, state, routingInfo, &convertToSigned16LE);
        default:
            return WrappedResult::wrapError("format is not supported");
    }
}

WrappedResult BassRouterDispatcher::routeNormal(void* in, void* out, std::size_t samples){
    TubeState state (order_);
    RoutingChannelInfo routingInfo (info_.normal, info_.bass, info_);

    switch (format_) {
        case TSimpleMessage::TStreamConfiguration::TSampleSpecification::FLOAT_32_BIG_ENDIAN:
            return typedRoute<std::uint32_t>(in, out, samples, state, routingInfo, &convertToFloat32BE);
        case TSimpleMessage::TStreamConfiguration::TSampleSpecification::FLOAT_32_LITTLE_ENDIAN:
            return typedRoute<std::uint32_t>(in, out, samples, state, routingInfo, &convertToFloat32LE);
        case TSimpleMessage::TStreamConfiguration::TSampleSpecification::UNSIGNED_8:
            return typedRoute<std::uint8_t>(in, out, samples, state, routingInfo, &convertToUnsigned8);
        case TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_32_BIG_ENDIAN:
            return typedRoute<std::uint32_t>(in, out, samples, state, routingInfo, &convertToSigned32BE);
        case TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_32_LITTLE_ENDIAN:
            return typedRoute<std::uint32_t>(in, out, samples, state, routingInfo, &convertToSigned32LE);
        case TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_16_BIG_ENDIAN:
            return typedRoute<std::uint16_t>(in, out, samples, state, routingInfo, &convertToSigned16BE);
        case TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_16_LITTLE_ENDIAN:
            return typedRoute<std::uint16_t>(in, out, samples, state, routingInfo, &convertToSigned16LE);
        default:
            return WrappedResult::wrapError("format is not supported");
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