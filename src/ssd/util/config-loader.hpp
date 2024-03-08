#pragma once

// nlohmann_json
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <nlohmann/json.hpp>

// standard
#include <functional>
#include <filesystem>
#include <string_view>
#include <cstdint>
#include <array>
#include <vector>

// common
#include <common/callback-queue.hpp>

namespace laar {

    using namespace std::filesystem;

    class ConfigHandler
        : public std::enable_shared_from_this<ConfigHandler> 
    {
    private: struct Private { };
    public:
        ConfigHandler(
            std::string_view configRootDirectory, 
            std::shared_ptr<laar::CallbackQueue> cbQueue,
            Private access
        );
        ~ConfigHandler();

        static std::shared_ptr<ConfigHandler> configure(
            std::string_view configRootDirectory, /* root directory to look for configs */
            std::shared_ptr<laar::CallbackQueue> cbQueue /* callback queue for syncing async API */
        );

        void init();
        void forceUpdate();

        void subscribeOnDefaultConfig(std::string section, std::function<void()> callback, std::weak_ptr<void> lifetime);
        void subscribeOnDynamicConfig(std::string section, std::function<void()> callback, std::weak_ptr<void> lifetime);

    private:

        void schedule();
        void notifyDefaultSubscribers();
        void notifyDynamicSubscribers();
        bool parseDefault();
        bool parseDynamic();

    private:
        const std::string_view configRootDirectory_;
        std::shared_ptr<laar::CallbackQueue> cbQueue_;
        
        struct ConfigFile {
            path filepath;
            file_time_type lastUpdatedTs;
            nlohmann::json contents;
        };

        struct Subscriber {
            std::function<void()> callback;
            std::weak_ptr<void> lifetime;
        };

        ConfigFile default_;
        ConfigFile dynamic_;

        std::vector<Subscriber> defaultDefaultSubscribers_;
        std::vector<Subscriber> dynamicDefaultSubscribers_;
    };

} // namespace util