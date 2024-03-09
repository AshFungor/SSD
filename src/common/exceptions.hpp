// standard
#include <exception>


namespace laar {

    class LaarBadInit : std::exception {
    public:
        virtual const char* what() const noexcept override {
            return "init() called more than once on created object";
        }
    };

    class LaarNoInit : std::exception {
    public:
        virtual const char* what() const noexcept override {
            return "init() was not called although object is about to be destroyed";
        }
    };

}