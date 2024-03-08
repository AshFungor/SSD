// nlohmann_json
#include "util/config-loader.hpp"
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <nlohmann/json.hpp>

// standard
#include <string_view>
#include <filesystem>
#include <functional>
#include <fstream>
#include <cstdint>
#include <vector>
#include <array>

// laar
#include <common/callback-queue.hpp>

using namespace laar;


std::shared_ptr<ConfigHandler> ConfigHandler::configure(
    std::string_view configRootDirectory,
    std::shared_ptr<laar::CallbackQueue> cbQueue)
{
    return std::make_shared<ConfigHandler>(configRootDirectory, cbQueue, Private());
}

ConfigHandler::ConfigHandler(
    std::string_view configRootDirectory, 
    std::shared_ptr<laar::CallbackQueue> cbQueue,
    Private access)
    : configRootDirectory_(configRootDirectory)
    , cbQueue_(cbQueue)
{}


// std::shared_ptr<ConfigHandler> configure(
//     std::string_view configRootDirectory,
//     std::shared_ptr<CallbackQueue> cbQueue) 
// {
    
// }

// ConfigHandler::ConfigHandler(std::string_view configRootDirectory, bool handling, std::shared_ptr<CallbackQueue> cbQueue)
// : configRootDirectory_(configRootDirectory)
// , handling(handling)
// , cbQueue_(std::move(cbQueue))
// {}

// ConfigHandler::~ConfigHandler() {
//     if (handling) { flush(); }
// }

// void ConfigHandler::init() {
//     update(true);

//     for (std::size_t numeric = 0; numeric < ConfigTypesNumber; ++numeric) {
//         auto& file = paths_[numeric];
//         if (supportedConfigs_[numeric]) {
//             file.ts = last_write_time(file.filepath);
//         }
//     }
// }

// void ConfigHandler::flush() {
//     cbQueue_->query([this]() mutable {
//         for (std::size_t numeric = 0; numeric < ConfigTypesNumber; ++numeric) {
//             if (supportedConfigs_[numeric]) { 
//                 auto result = write(static_cast<cfg>(numeric)); 
//                 if (!result) {
//                     supportedConfigs_[numeric] = false;
//                 }
//             }
//         }
//     });
// }

// void ConfigHandler::update(bool force) {
//     for (std::size_t numeric = 0; numeric < ConfigTypesNumber; ++numeric) {
//         if (supportedConfigs_[numeric]) {
//             if (!isModified(static_cast<cfg>(numeric)) && !force) {
//                 continue;
//             }
//             auto result = parse(static_cast<cfg>(numeric));
//             if (!result) {
//                 supportedConfigs_[numeric] = false;
//             }
//             notify(static_cast<cfg>(numeric));
//         }
//     }
// }

// void ConfigHandler::notify(cfg target) {
//     for (const auto& callback : endpoints_[static_cast<std::size_t>(target)]) {
//         callback();
//     }
// }

// bool ConfigHandler::isModified(cfg target) {
//     auto& configFile = paths_[static_cast<std::size_t>(target)];
//     auto newTs = last_write_time(configFile.filepath);
//     if (newTs > configFile.ts) {
//         configFile.ts = newTs;
//         return true;
//     }
//     return false;
// }

// bool ConfigHandler::parse(cfg target) {
//     auto& configFile = paths_[static_cast<std::size_t>(target)];
//     auto& config = configs_[static_cast<std::size_t>(target)];

//     if (!exists(configFile.filepath)) {
//         return false;
//     } 

//     std::ifstream ifs {configFile.filepath, std::ios::in | std::ios::binary};
//     std::string jsonString;
//     std::size_t configFileSize = file_size(configFile.filepath);
//     jsonString.resize(configFileSize);
//     if (!ifs || ifs.read(jsonString.data(), configFileSize).gcount() < configFileSize) {
//         return false;
//     }

//     config = nlohmann::json::parse(jsonString);
//     return true;
// }

// bool ConfigHandler::write(cfg target) {
//     auto& configFile = paths_[static_cast<std::size_t>(target)];
//     auto& config = configs_[static_cast<std::size_t>(target)];

//     if (!exists(configFile.filepath)) {
//         return false;
//     } 

//     std::ofstream ofs {configFile.filepath, std::ios::out | std::ios::binary};
//     std::string jsonString = config.dump();
//     if (!ofs || !ofs.write(jsonString.data(), jsonString.size())) {
//         return false;
//     }

//     return true;
// }

// bool ConfigHandler::isSupported(cfg type) const {
//     return supportedConfigs_[static_cast<std::size_t>(type)];
// }

// const nlohmann::json& ConfigHandler::get(cfg type) const {
//     return configs_[static_cast<std::size_t>(type)];
// }

// const nlohmann::json& ConfigHandler::operator[](cfg type) const {
//     return get(type);
// }

// void ConfigHandler::subscribeOnConfig(cfg target, std::function<void()> callback) {
//     endpoints_[static_cast<std::size_t>(target)].push_back(callback);
// }