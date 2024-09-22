#pragma once

// abseil
#include <absl/status/status.h>

namespace laar {

    class IDispatcher {
    public:

        virtual absl::Status dispatch(void* in, void* out, std::size_t samples) = 0;
        virtual ~IDispatcher() = default;

    };

}
