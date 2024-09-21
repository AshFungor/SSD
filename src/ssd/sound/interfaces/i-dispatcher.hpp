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
        bool isError() const { 
            return result_.has_value(); 
        }

        const std::string& getError() const { 
            return result_.value(); 
        }

        WrappedResult operator&(const WrappedResult& other) const {
            if (other.isError() && isError()) {
                return WrappedResult::wrapError("joined errors: " + other.getError() + "; " + getError());
            } else if (other.isError()) {
                return other;
            } else if (isError()) {
                return *this;
            }

            return WrappedResult::wrapResult();
        }

        const WrappedResult& operator&=(const WrappedResult& other) {
            *this = operator&(other);
            return *this;
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
