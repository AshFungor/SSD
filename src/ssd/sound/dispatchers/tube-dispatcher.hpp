#pragma once

// abseil
#include <absl/status/status.h>

// STD
#include <memory>
#include <cstddef>
#include <cstdint>
#include <iterator>

// Local
#include <src/ssd/sound/converter.hpp>
#include <src/ssd/sound/interfaces/i-dispatcher.hpp>


namespace laar {

    enum class ESamplesOrder : std::int_fast8_t {
        INTERLEAVED, NONINTERLEAVED
    };

    template<typename SampleType>
    class ChannelIterator {
    public:

        using difference_type = std::ptrdiff_t;
        using value_type = SampleType;
        using pointer = SampleType*;
        using reference = SampleType&;
        using iterator_category = std::bidirectional_iterator_tag;

        ChannelIterator(
            std::uint32_t samples,
            std::uint32_t channels,
            std::uint32_t channel,
            ESamplesOrder order,
            pointer buffer,
            std::uint32_t position
        );

        ChannelIterator& operator++();
        ChannelIterator& operator--();

        ChannelIterator& operator++(int);
        ChannelIterator& operator--(int);

        bool operator==(const ChannelIterator& other) noexcept;
        bool operator!=(const ChannelIterator& other) noexcept;

        reference operator*();

    private:
        pointer advance(std::int32_t distance);
        difference_type shift();

    private:
        const std::uint32_t samples_;
        const std::uint32_t channels_;
        const std::uint32_t channel_;
        const ESamplesOrder order_;

        pointer buffer_;
        pointer position_;
    
    };

    template<typename SampleType>
    class StreamWrapper {
    public:

        StreamWrapper(
            std::uint32_t samples,
            std::uint32_t channels,
            ESamplesOrder order,
            SampleType* buffer
        );

        ChannelIterator<SampleType> begin(std::uint32_t channel = 0) noexcept;
        ChannelIterator<SampleType> end(std::uint32_t channel = 0) noexcept;

        ChannelIterator<SampleType> at(std::uint32_t sample, std::uint32_t channel = 0) noexcept;

    private:
        const std::uint32_t samples_;
        const std::uint32_t channels_;
        const ESamplesOrder order_;
        SampleType* buffer_;

    };

    // const iterator variation
    template<typename SampleType>
    using ConstChannelIterator = ChannelIterator<const SampleType>;

    // Dispatches incoming channels into one, or one-to-many
    class TubeDispatcher 
        : IDispatcher
        , std::enable_shared_from_this<TubeDispatcher> {
    private: struct Private { };
    public:

        enum class EDispatchingDirection : std::int_fast8_t {
            ONE2MANY, MANY2ONE
        };

        static std::shared_ptr<TubeDispatcher> create(
            const EDispatchingDirection& dir, 
            const ESamplesOrder& order, 
            const ESampleType& format,
            const std::size_t& channels
        );

        TubeDispatcher(
            const EDispatchingDirection& dir, 
            const ESamplesOrder& order, 
            const ESampleType& format,
            const std::size_t& channels,
            Private access
        );

        // IDispatcher implementation
        absl::Status dispatch(void* in, void* out, std::size_t samples) override;

    private:
        absl::Status dispatchOneToMany(void* in, void* out, std::size_t samples) noexcept;
        absl::Status dispatchManyToOne(void* in, void* out, std::size_t samples) noexcept;

    private:
        const EDispatchingDirection dir_;
        const ESamplesOrder order_;
        const ESampleType format_;
        const std::size_t channels_;

    };

}


template<typename SampleType>
laar::ChannelIterator<SampleType>::ChannelIterator(
    std::uint32_t samples,
    std::uint32_t channels,
    std::uint32_t channel,
    ESamplesOrder order,
    pointer buffer,
    std::uint32_t position
)
    : samples_(samples)
    , channels_(channels)
    , channel_(channel)
    , order_(order)
    , buffer_(buffer)
    , position_(buffer)
{
    advance(position);
}

template<typename SampleType>
laar::ChannelIterator<SampleType>& laar::ChannelIterator<SampleType>::operator++() {
    position_ = advance(1);
    return *this;
}

template<typename SampleType>
laar::ChannelIterator<SampleType>& laar::ChannelIterator<SampleType>::operator++(int) {
    auto frozen = *this;
    position_ = advance(1);
    return frozen;
}

template<typename SampleType>
laar::ChannelIterator<SampleType>& laar::ChannelIterator<SampleType>::operator--() {
    position_ = advance(-1);
    return *this;
}

template<typename SampleType>
laar::ChannelIterator<SampleType>& laar::ChannelIterator<SampleType>::operator--(int) {
    auto frozen = *this;
    position_ = advance(-1);
    return frozen;
}

template<typename SampleType>
bool laar::ChannelIterator<SampleType>::operator==(const ChannelIterator& other) noexcept {
    return (
        samples_ == other.samples_ &&
        channels_ == other.channels_ &&
        channel_ == other.channel_ &&
        order_ == other.order_
    ) && (
        buffer_ == other.buffer_ &&
        position_ == other.position_
    );
}

template<typename SampleType>
bool laar::ChannelIterator<SampleType>::operator!=(const ChannelIterator& other) noexcept {
    return !operator==(other);
}

template<typename SampleType>
laar::ChannelIterator<SampleType>::reference laar::ChannelIterator<SampleType>::operator*() {
    return *position_;
}

template<typename SampleType>
SampleType* laar::ChannelIterator<SampleType>::advance(std::int32_t distance) {
    if ((distance + shift() > samples_ && distance > 0) || (shift() + distance < 0 && distance < 0)) {
        throw std::runtime_error("channel read out of bounds");
    }

    switch (order_) {
        case laar::ESamplesOrder::INTERLEAVED:
            if (buffer_ == position_) {
                position_ += channel_;
            } 
            position_ += channels_ * distance;
            break;
        case laar::ESamplesOrder::NONINTERLEAVED:
            if (buffer_ == position_) {
                position_ += samples_ * channel_;
            }
            position_ += distance;
            break;
    }

    return &operator*();
}

template<typename SampleType>
ptrdiff_t laar::ChannelIterator<SampleType>::shift() {
    switch (order_) {
        case laar::ESamplesOrder::INTERLEAVED:
            return static_cast<ptrdiff_t>(position_ - channel_ - buffer_) / channels_;
        case laar::ESamplesOrder::NONINTERLEAVED:
            return static_cast<ptrdiff_t>(position_ - buffer_ - channel_ * samples_);
        default:
            return 0;
    }
}


template<typename SampleType>
laar::StreamWrapper<SampleType>::StreamWrapper(
    std::uint32_t samples,
    std::uint32_t channels,
    ESamplesOrder order,
    SampleType* buffer
)
    : samples_(samples)
    , channels_(channels)
    , order_(order)
    , buffer_(buffer)
{}


template<typename SampleType>
laar::ChannelIterator<SampleType> laar::StreamWrapper<SampleType>::begin(std::uint32_t channel) noexcept {
    return ChannelIterator<SampleType>(
        samples_, channels_, channel, order_, buffer_, 0
    );
}


template<typename SampleType>
laar::ChannelIterator<SampleType> laar::StreamWrapper<SampleType>::end(std::uint32_t channel) noexcept {
    return ChannelIterator<SampleType>(
        samples_, channels_, channel, order_, buffer_, samples_
    );
}


template<typename SampleType>
laar::ChannelIterator<SampleType> laar::StreamWrapper<SampleType>::at(std::uint32_t sample, std::uint32_t channel) noexcept {
    return ChannelIterator<SampleType>(
        samples_, channels_, channel, order_, buffer_, sample
    );
}