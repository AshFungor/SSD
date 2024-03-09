#pragma once

// nlohmann_json
#include <nlohmann/json_fwd.hpp>
#include <nlohmann/json.hpp>

// standard
#include <functional>
#include <filesystem>
#include <string_view>
#include <vector>
#include <memory>

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
        void update();

        void subscribeOnDefaultConfig(const std::string& section, std::function<void(const nlohmann::json&)> callback, std::weak_ptr<void> lifetime);
        void subscribeOnDynamicConfig(const std::string& section, std::function<void(const nlohmann::json&)> callback, std::weak_ptr<void> lifetime);

    private:
        struct Subscriber;

        void schedule();
        void notify(const Subscriber& sub, const nlohmann::json& config);
        void notifyDefaultSubscribers();
        void notifyDynamicSubscribers();
        bool parseDefault();
        bool parseDynamic();

    private:
        std::shared_ptr<laar::CallbackQueue> cbQueue_;
        
        struct ConfigFile {
            path filepath;
            file_time_type lastUpdatedTs;
            nlohmann::json contents;
        };

        struct Subscriber {
            std::string section;
            std::function<void(const nlohmann::json&)> callback;
            std::weak_ptr<void> lifetime;
        };

        ConfigFile default_;
        ConfigFile dynamic_;

        bool isDynamicAvailable_;
        bool isDefaultAvailable_;

        std::vector<Subscriber> defaultConfigSubscribers_;
        std::vector<Subscriber> dynamicConfigSubscribers_;
    };

} // namespace util