// nlohmann_json
#include <nlohmann/json_fwd.hpp>
#include <nlohmann/json.hpp>

// standard
#include <plog/Severity.h>
#include <string_view>
#include <filesystem>
#include <functional>
#include <iostream>
#include <fstream>
#include <chrono>
#include <memory>

// plog
#include <plog/Log.h>

// laar
#include <common/callback-queue.hpp>

// local
#include "util/config-loader.hpp"

using namespace laar;

namespace {
    std::string makeDefaultConfigPath(std::string_view directory) {
        return directory.data() + std::string("default.cfg");
    }
    std::string makeDynamicConfigPath(std::string_view directory) {
        return directory.data() + std::string("dynamic.cfg");
    }
}


std::shared_ptr<ConfigHandler> ConfigHandler::configure(
    std::string_view configRootDirectory,
    std::shared_ptr<laar::CallbackQueue> cbQueue)
{
    return std::make_shared<ConfigHandler>(configRootDirectory, std::move(cbQueue), Private());
}

ConfigHandler::ConfigHandler(
    std::string_view configRootDirectory, 
    std::shared_ptr<laar::CallbackQueue> cbQueue,
    Private access)
    : cbQueue_(std::move(cbQueue))
    , isDefaultAvailable_(false)
    , isDynamicAvailable_(false)
    , default_(ConfigFile{
        .filepath = makeDefaultConfigPath(configRootDirectory),
        .lastUpdatedTs = {},
        .contents = {}
    })
    , dynamic_(ConfigFile{
        .filepath = makeDynamicConfigPath(configRootDirectory),
        .lastUpdatedTs = {},
        .contents = {}
    })
{}

ConfigHandler::~ConfigHandler() {}

void ConfigHandler::init() {
    isDefaultAvailable_ = parseDefault();
    isDynamicAvailable_ = parseDynamic();

    PLOG(plog::debug) << "default config received: " << default_.contents.dump();
    PLOG(plog::debug) << "dynamic config received: " << dynamic_.contents.dump();

    dynamic_.lastUpdatedTs = last_write_time(dynamic_.filepath);
    default_.lastUpdatedTs = last_write_time(default_.filepath);

    schedule();
}

void ConfigHandler::update() {
    cbQueue_->query([this]() {
        if (auto newTs = last_write_time(dynamic_.filepath); newTs > dynamic_.lastUpdatedTs) {
            dynamic_.lastUpdatedTs = newTs;
            isDynamicAvailable_ = parseDynamic();
            if (isDynamicAvailable_) notifyDynamicSubscribers();
        }
        schedule();
    });
}

void ConfigHandler::schedule() {
    cbQueue_->query([ptr = weak_from_this()]() {
        if (auto handler = ptr.lock()) handler->update();
    }, std::chrono::milliseconds(500));
}

bool ConfigHandler::parseDefault() {
    std::ifstream ifs {default_.filepath, std::ios::in | std::ios::binary};
    std::string jsonString;
    std::size_t configFileSize = file_size(default_.filepath);
    jsonString.resize(configFileSize);
    if (!ifs || ifs.read(jsonString.data(), configFileSize).gcount() < configFileSize) {
        return false;
    }

    default_.contents = nlohmann::json::parse(jsonString);
    return true;
}

bool ConfigHandler::parseDynamic() {
    std::ifstream ifs {dynamic_.filepath, std::ios::in | std::ios::binary};
    std::string jsonString;
    std::size_t configFileSize = file_size(dynamic_.filepath);
    jsonString.resize(configFileSize);
    if (!ifs || ifs.read(jsonString.data(), configFileSize).gcount() < configFileSize) {
        return false;
    }

    dynamic_.contents = nlohmann::json::parse(jsonString);
    return true;
}

void ConfigHandler::notify(const Subscriber& sub, const nlohmann::json& config) {
    if (!config.contains(sub.section)) {
        if (!sub.lifetime.lock()) return;
        sub.callback(config[sub.section]);
    }
}

void ConfigHandler::notifyDefaultSubscribers() {
    for (const auto& sub : defaultConfigSubscribers_) {
        notify(sub, default_.contents);
    }
}

void ConfigHandler::notifyDynamicSubscribers() {
    for (const auto& sub : dynamicConfigSubscribers_) {
        notify(sub, dynamic_.contents);
    }
}

void ConfigHandler::subscribeOnDefaultConfig(
    const std::string& section, 
    std::function<void(const nlohmann::json&)> callback, 
    std::weak_ptr<void> lifetime) 
{
    cbQueue_->query([ptr = weak_from_this(), section, callback, lifetime]() {
        if (auto handler = ptr.lock()) {
            handler->defaultConfigSubscribers_.push_back(Subscriber{
                .section = section,
                .callback = std::move(callback),
                .lifetime = std::move(lifetime)
            });
            handler->notify(handler->defaultConfigSubscribers_.back(), handler->default_.contents);
        }
    });
}

void ConfigHandler::subscribeOnDynamicConfig(
    const std::string& section, 
    std::function<void(const nlohmann::json&)> callback, 
    std::weak_ptr<void> lifetime)
{
    cbQueue_->query([ptr = weak_from_this(), section, callback, lifetime]() {
        if (auto handler = ptr.lock()) {
            handler->dynamicConfigSubscribers_.push_back(Subscriber{
                .section = section,
                .callback = std::move(callback),
                .lifetime = std::move(lifetime)
            });
            handler->notify(handler->dynamicConfigSubscribers_.back(), handler->dynamic_.contents);
        }
    });
}