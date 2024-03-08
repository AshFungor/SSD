// standard
#include <exception>
#include <stdexcept>


namespace laar {

    class LaarBadInit : std::exception {
    public:
        virtual const char* what() const noexcept override {
            return "init() called more than once on created object";
        }
    };

}