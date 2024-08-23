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
        fftw_complex* original = static_cast<fftw_complex*>(in);

        ResultingSampleType* resulting = static_cast<ResultingSampleType*>(out);
        StreamWrapper<ResultingSampleType> outWrappedStream(
            samples, routingInfo.channelNum, state.order_, resulting 
        );

        auto outCurrent = outWrappedStream.begin(routingInfo.routeTo);
        for (std::size_t sample = 0; sample < samples; ++sample) {
            if (outCurrent == outWrappedStream.end(routingInfo.routeTo)) {
                return WrappedResult::wrapError("reached end of out stream unexpectedly");
            }
            *outCurrent = process(static_cast<std::int32_t>(original[sample][0] / samples));
            ++outCurrent;
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

    for (int i = 0; i < samples / 2; ++i) {
        double freq = i * resolution;

        if (freq >= range_.lower && freq <= range_.higher) {
            frequencies.bass[i][0] = complexOut[i][0];
            frequencies.bass[i][1] = complexOut[i][1];
            frequencies.normal[i][0] = 0;
            frequencies.normal[i][1] = 0; 
        } else {
            frequencies.bass[i][0] = 0;
            frequencies.bass[i][1] = 0;
            frequencies.normal[i][0] = complexOut[i][0];
            frequencies.normal[i][1] = complexOut[i][1];
        }
    }

    fftw_plan normalBackwardPlan = fftw_plan_dft_1d(samples, frequencies.normal, complexIn, FFTW_BACKWARD, FFTW_ESTIMATE);
    fftw_execute(normalBackwardPlan);

    fftw_plan bassBackwardPlan = fftw_plan_dft_1d(samples, frequencies.bass, complexOut, FFTW_BACKWARD, FFTW_ESTIMATE);
    fftw_execute(bassBackwardPlan);

    fftw_free(frequencies.normal);
    fftw_free(frequencies.bass);

    fftw_destroy_plan(p);
    fftw_destroy_plan(normalBackwardPlan);
    fftw_destroy_plan(bassBackwardPlan);

    frequencies.normal = complexIn;
    frequencies.bass = complexOut;

    return frequencies;
}

WrappedResult BassRouterDispatcher::dispatch(void* in, void* out, std::size_t samples) {
    auto data = splitWindow(reinterpret_cast<std::int32_t*>(in), sampleRate_, samples);
    if (auto bassDispatching = routeBass(data.bass, out, samples); bassDispatching.isError()) {
        fftw_free(data.normal);
        fftw_free(data.bass);
        return bassDispatching;
    }
    fftw_free(data.normal);
    fftw_free(data.bass);
    return routeNormal(data.normal, out, samples);
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