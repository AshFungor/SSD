// STD
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

// common
#include <common/callback.hpp>

namespace sound {

    class IStream {
    public:

        enum class EOperation {
            READ, WRITE
        };

        class IIOOperation {
        public:
            const EOperation operation;
            void* buffer;
            std::size_t size;
        };

        class IIOCallback : public laar::ICallback<std::unique_ptr<IIOOperation>> {
        public:
            // May add something
        };

        virtual ~IStream() = 0;
        // Non-blocking API
        virtual std::unique_ptr<IIOOperation>& poll(EOperation op) = 0;
        virtual void registerCallback(EOperation op, const IIOCallback& callback);

    private:
        virtual void write(const char* data, std::size_t) = 0;
        virtual void read(char* data, std::size_t size) = 0;

    };

}