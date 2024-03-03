#pragma once

// nlohmann_json
#include <nlohmann/json_fwd.hpp>
#include <nlohmann/json.hpp>

// standard
#include <functional>
#include <filesystem>
#include <string_view>
#include <cstdint>
#include <array>
#include <vector>

// local
#include "callback-queue.hpp"

namespace util {

    using namespace std::filesystem;

    constexpr std::size_t ConfigTypesNumber = 2;

    enum class cfg : std::size_t {
        BASE, 
        DYNAMIC /* support for renewing config will be added later */
    };

    class ConfigHandler {
    public:
        ConfigHandler(std::string_view configRootDirectory, bool handling, std::shared_ptr<CallbackQueue> cbQueue);
        ~ConfigHandler();

        void init();
        void flush();
        void update(bool force = false);
        // callback must be thread-safe.
        void subscribeOnConfig(cfg target, std::function<void()> callback);

        const nlohmann::json& get(cfg type) const;
        const nlohmann::json& operator[](cfg type) const;

        // this is not thread aware, must be synced by caller 
        nlohmann::json& modify(cfg type);

        bool isSupported(cfg type) const;

    private:
        bool isModified(cfg target);
        bool write(cfg target);
        void notify(cfg target);
        bool parse(cfg target);

    private:
        bool handling; 
        const std::string configRootDirectory_;
        std::shared_ptr<CallbackQueue> cbQueue_;
        std::array<bool, ConfigTypesNumber> supportedConfigs_ = {
            true, /* default config */
            false /* dynamic config */
        };
        std::array<nlohmann::json, ConfigTypesNumber> configs_;

        struct ConfigFile {
            path filepath;
            file_time_type ts;
        };

        std::array<ConfigFile, ConfigTypesNumber> paths_ {
            ConfigFile{.filepath = configRootDirectory_ + "default.cfg"},
            ConfigFile{.filepath = configRootDirectory_ + "dynamic.cfg"}
        };

        using Endpoints = std::vector<std::function<void()>>;
        std::array<Endpoints, ConfigTypesNumber> endpoints_;
    };

} // namespace util