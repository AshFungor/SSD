// abseil
#include <absl/status/status.h>

// protos
#include <protos/client/stream.pb.h>

// STD
#include <cstdint>

// Local
#include <src/ssd/sound/converter.hpp>
#include <src/ssd/sound/interfaces/i-dispatcher.hpp>
#include <src/ssd/sound/dispatchers/tube-dispatcher.hpp>

using namespace laar;
using ESamples = NSound::NCommon::TStreamConfiguration::TSampleSpecification;


namespace {

    struct TubeState {
        TubeState(const ESamplesOrder& order, const std::size_t& channels)
            : order_(order)
            , channels_(channels)
        {}

        const ESamplesOrder order_;
        const std::size_t channels_;
    };

    template<typename ResultingSampleType, typename FunctorType>
    absl::Status typedDispatchOneToMany(void* in, void* out, std::size_t samples, TubeState state, FunctorType process) {
        std::int32_t* original = static_cast<std::int32_t*>(in);
        StreamWrapper<std::int32_t> inWrappedStream(
            samples, 1, state.order_, original 
        );

        ResultingSampleType* resulting = static_cast<ResultingSampleType*>(out);
        StreamWrapper<ResultingSampleType> outWrappedStream(
            samples, state.channels_, state.order_, resulting 
        );

        std::vector<ChannelIterator<ResultingSampleType>> channelIters;
        for (std::size_t channel = 0; channel < state.channels_; ++channel) {
            channelIters.push_back(outWrappedStream.begin(channel));
        }

        for (auto inCurrent = inWrappedStream.begin(); inCurrent != inWrappedStream.end(); ++inCurrent) {
            for (auto& outCurrent : channelIters) {
                *outCurrent = process(*inCurrent);
                ++outCurrent;
            }
        }

        return absl::OkStatus();
    }

    template<typename IncomingSampleType, typename FunctorType>
    absl::Status typedDispatchManyToOne(void* in, void* out, std::size_t samples, TubeState state, FunctorType process) {
        std::int32_t* resulting = static_cast<std::int32_t*>(out);
        StreamWrapper<std::int32_t> outWrappedStream(
            samples, 1, state.order_, resulting 
        );

        IncomingSampleType* incoming = static_cast<IncomingSampleType*>(in);
        StreamWrapper<IncomingSampleType> inWrappedStream(
            samples, state.channels_, state.order_, incoming 
        );

        std::vector<ChannelIterator<IncomingSampleType>> channelIters;
        for (std::size_t channel = 0; channel < state.channels_; ++channel) {
            channelIters.push_back(inWrappedStream.begin(channel));
        }

        for (auto outCurrent = outWrappedStream.begin(); outCurrent != outWrappedStream.end(); ++outCurrent) {
            bool isFirst = true;
            for (auto& inCurrent : channelIters) {
                if (isFirst) {
                    *outCurrent = process(*inCurrent);
                    isFirst = false;
                } else {
                    std::int32_t processed = process(*inCurrent);
                    *outCurrent = 2 * ((std::int64_t) *outCurrent + processed) 
                        - (long double) *outCurrent / (INT32_MAX / 2) * processed - INT32_MAX;
                }
                ++inCurrent;
            }
        }

        return absl::OkStatus();
    }

}


absl::Status TubeDispatcher::dispatch(void* in, void* out, std::size_t samples) {
    switch (dir_) {
        case EDispatchingDirection::MANY2ONE:
            return dispatchManyToOne(in, out, samples);
        case EDispatchingDirection::ONE2MANY:
            return dispatchOneToMany(in, out, samples);
        default:
            return absl::InvalidArgumentError("unsupported direction");
    }
}

absl::Status TubeDispatcher::dispatchOneToMany(void* in, void* out, std::size_t samples) noexcept {
    TubeState state (order_, channels_);

    switch (format_) {
        case ESamples::FLOAT_32_BIG_ENDIAN:
            return typedDispatchOneToMany<std::uint32_t>(in, out, samples, state, &convertToFloat32BE);
        case ESamples::FLOAT_32_LITTLE_ENDIAN:
            return typedDispatchOneToMany<std::uint32_t>(in, out, samples, state, &convertToFloat32LE);
        case ESamples::UNSIGNED_8:
            return typedDispatchOneToMany<std::uint8_t>(in, out, samples, state, &convertToUnsigned8);
        case ESamples::SIGNED_32_BIG_ENDIAN:
            return typedDispatchOneToMany<std::uint32_t>(in, out, samples, state, &convertToSigned32BE);
        case ESamples::SIGNED_32_LITTLE_ENDIAN:
            return typedDispatchOneToMany<std::uint32_t>(in, out, samples, state, &convertToSigned32LE);
        case ESamples::SIGNED_16_BIG_ENDIAN:
            return typedDispatchOneToMany<std::uint16_t>(in, out, samples, state, &convertToSigned16BE);
        case ESamples::SIGNED_16_LITTLE_ENDIAN:
            return typedDispatchOneToMany<std::uint16_t>(in, out, samples, state, &convertToSigned16LE);
        default:
            return absl::InvalidArgumentError("format is not supported");
    }
}

absl::Status TubeDispatcher::dispatchManyToOne(void* in, void* out, std::size_t samples) noexcept {
    TubeState state (order_, channels_);

    switch (format_) {
        case ESamples::FLOAT_32_BIG_ENDIAN:
            return typedDispatchManyToOne<std::uint32_t>(in, out, samples, state, &convertFromFloat32BE);
        case ESamples::FLOAT_32_LITTLE_ENDIAN:
            return typedDispatchManyToOne<std::uint32_t>(in, out, samples, state, &convertFromFloat32LE);
        case ESamples::UNSIGNED_8:
            return typedDispatchManyToOne<std::uint8_t>(in, out, samples, state, &convertFromUnsigned8);
        case ESamples::SIGNED_32_BIG_ENDIAN:
            return typedDispatchManyToOne<std::uint32_t>(in, out, samples, state, &convertFromSigned32BE);
        case ESamples::SIGNED_32_LITTLE_ENDIAN:
            return typedDispatchManyToOne<std::uint32_t>(in, out, samples, state, &convertFromSigned32LE);
        case ESamples::SIGNED_16_BIG_ENDIAN:
            return typedDispatchManyToOne<std::uint16_t>(in, out, samples, state, &convertFromSigned16BE);
        case ESamples::SIGNED_16_LITTLE_ENDIAN:
            return typedDispatchManyToOne<std::uint16_t>(in, out, samples, state, &convertFromSigned16LE);
        default:
            return absl::InvalidArgumentError("format is not supported");
    }
}

TubeDispatcher::TubeDispatcher(
    const EDispatchingDirection& dir, 
    const ESamplesOrder& order, 
    const ESampleType& format,
    const std::size_t& channels,
    Private /* access */
)
    : dir_(dir)
    , order_(order)
    , format_(format)
    , channels_(channels)
{}

std::shared_ptr<TubeDispatcher> TubeDispatcher::create(
    const EDispatchingDirection& dir, 
    const ESamplesOrder& order, 
    const ESampleType& format,
    const std::size_t& channels
) {
    return std::make_shared<TubeDispatcher>(dir, order, format, channels, Private());
}