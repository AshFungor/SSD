#pragma once

// STD
#include <cstdint>
#include <memory>

// protos
#include <protos/client/simple/simple.pb.h>

// Local
#include <sounds/interfaces/i-dispatcher.hpp>
#include <variant>
#include <vector>


namespace laar {

    // Dispatches incoming channels into one, or one-to-many
    class TubeDispatcher 
        : IDispatcher<SimpleErrorHolder>
        , std::enable_shared_from_this<TubeDispatcher> {
    public:

        using TSampleSpec = NSound::NSimple::TSimpleMessage::TStreamConfiguration::TSampleSpecification;

        enum class EDispatchingDirection : std::int_fast8_t {
            ONE2MANY, MANY2ONE
        };

        enum class ESamplesOrder : std::int_fast8_t {
            INTERLEAVED, NONINTERLEAVED
        };

        TubeDispatcher(
            EDispatchingDirection dir, 
            ESamplesOrder order, 
            std::variant<TSampleSpec, std::vector<TSampleSpec>> spec
        );

        // IDispatcher implementation
        IErrorHolder dispatch(void* in, void* out) override;

    private:

        void getChannel(void* in, TSampleSpec spec, std::int32_t* buffer);
        std::int32_t balanceSample(std::int32_t first, void* second, TSampleSpec spec);
        void writeFormatted(void* out, std::int32_t sample, TSampleSpec spec);
        
    };

}