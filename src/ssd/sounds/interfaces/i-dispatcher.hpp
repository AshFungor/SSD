#pragma once

// STD
#include <optional>
#include <string>


namespace laar {

    class WrappedResult {
    public:
        // result builders
        static WrappedResult wrapError(const std::string& what) { 
            return WrappedResult(what); 
        }

        static WrappedResult wrapResult() { 
            return WrappedResult(std::nullopt); 
        }

        // result state
        bool isError() { 
            return result_.has_value(); 
        }

        std::string& getError() { 
            return result_.value(); 
        }

    private:
        WrappedResult(std::optional<std::string> result) : result_(std::move(result)) {}

    private:
        std::optional<std::string> result_;

    };

    class IDispatcher {
    public:

        virtual WrappedResult dispatch(void* in, void* out, std::size_t samples) = 0;
        virtual ~IDispatcher() = default;

    };

}
