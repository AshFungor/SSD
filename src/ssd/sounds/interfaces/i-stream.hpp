// STD
#include <functional>
#include <memory>

// common
#include <common/callback.hpp>

namespace sound {

    class IStream {
    public:

        virtual ~IStream() = 0;
        // Non-blocking API
        virtual std::size_t pollRead() = 0;
        virtual std::size_t pollWrite() = 0;

    };

}