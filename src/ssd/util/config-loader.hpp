#pragma once

// boost
#include <boost/asio/executor.hpp>
#include <boost/asio/high_resolution_timer.hpp>

// Abseil
#include <absl/status/status.h>

// nlohmann_json
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

// standard
#include <mutex>
#include <vector>
#include <memory>
#include <functional>
#include <filesystem>
#include <string_view>


namespace laar {

    class ConfigHandler
        : public std::enable_shared_from_this<ConfigHandler> {
    public:

        static std::shared_ptr<ConfigHandler> configure(
            std::string_view configRootDirectory,
            std::shared_ptr<boost::asio::io_context> context
        );

        absl::Status init();

        void subscribeOnDefaultConfig(const std::string& section, std::function<void(const nlohmann::json&)> callback, std::weak_ptr<void> lifetime);
        void subscribeOnDynamicConfig(const std::string& section, std::function<void(const nlohmann::json&)> callback, std::weak_ptr<void> lifetime);

    private:

        struct ConfigFile {
            std::filesystem::path filepath;
            std::filesystem::file_time_type lastUpdatedTs;
            nlohmann::json contents;

            bool isAvailable;
        };

        struct Subscriber {
            std::string section;
            std::function<void(const nlohmann::json&)> callback;
            std::weak_ptr<void> lifetime;
        };

        ConfigHandler(
            std::string_view configRootDirectory, 
            std::shared_ptr<boost::asio::io_context> context
        );

        void schedule(boost::system::error_code error);

        void notify(const Subscriber& sub, const nlohmann::json& config);
        void notifySubscribers(std::vector<Subscriber> subs, ConfigFile& config);
        absl::Status parseConfig(ConfigFile& config);

    private:
        std::mutex lock_;
        std::once_flag init_;

        std::shared_ptr<boost::asio::io_context> context_;
        std::unique_ptr<boost::asio::high_resolution_timer> timer_;

        ConfigFile default_;
        ConfigFile dynamic_;

        std::vector<Subscriber> defaultConfigSubscribers_;
        std::vector<Subscriber> dynamicConfigSubscribers_;
    };

} // namespace util