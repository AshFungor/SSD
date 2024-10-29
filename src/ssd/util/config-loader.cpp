// boost
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/system/detail/errc.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/asio/high_resolution_timer.hpp>

// Abseil
#include <absl/status/status.h>
#include <absl/strings/str_cat.h>

// nlohmann_json
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

// standard
#include <mutex>
#include <chrono>
#include <memory>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <functional>
#include <string_view>

// plog
#include <plog/Severity.h>
#include <plog/Log.h>

// laar
#include <src/ssd/util/config-loader.hpp>

using namespace laar;
using namespace std::chrono;

namespace {

    constexpr std::chrono::milliseconds timeout = 500ms;

}

std::shared_ptr<ConfigHandler> ConfigHandler::configure(
    std::string_view configRootDirectory,
    std::shared_ptr<boost::asio::io_context> context
) {
    return std::shared_ptr<ConfigHandler>(new ConfigHandler{configRootDirectory, std::move(context)});
}

ConfigHandler::ConfigHandler(
    std::string_view configRootDirectory, 
    std::shared_ptr<boost::asio::io_context> context
)
    : context_(std::move(context))
    , default_(ConfigFile{
        .filepath = absl::StrCat(configRootDirectory, "/default.cfg"),
        .lastUpdatedTs = {},
        .contents = {},
        .isAvailable = false
    })
    , dynamic_(ConfigFile{
        .filepath = absl::StrCat(configRootDirectory, "/dynamic.cfg"),
        .lastUpdatedTs = {},
        .contents = {},
        .isAvailable = false
    })
{}

absl::Status ConfigHandler::init() {
    absl::Status status;
    std::call_once(init_, [this, &status]() {
        status = parseConfig(default_);
        status.Update(parseConfig(dynamic_));

        dynamic_.lastUpdatedTs = last_write_time(dynamic_.filepath);
        default_.lastUpdatedTs = last_write_time(default_.filepath);

        if (status.ok()) {
            schedule(boost::system::errc::make_error_code(boost::system::errc::success));
        }
    });

    return status;
}

void ConfigHandler::schedule(boost::system::error_code error) {
    if (error) {
        PLOG(plog::error) 
            << "error on scheduled config update call: " << error.message()
            << "; aborting execution";
    }

    if (timer_) {
        // not first to call to schedule
        if (auto newTs = last_write_time(dynamic_.filepath); newTs > dynamic_.lastUpdatedTs) {
            dynamic_.lastUpdatedTs = newTs;
            absl::Status status = parseConfig(dynamic_);
            if (status.ok()) {
                notifySubscribers(dynamicConfigSubscribers_, dynamic_);
            } else {
                PLOG(plog::warning) 
                    << "error while updating config: " << status.message()
                    << "; cancelling scheduling";
                return;
            }
        }
    }

    // make sure call returns until next scheduling
    boost::asio::post(*context_, [this]() {
        timer_ = std::make_unique<boost::asio::high_resolution_timer>(*context_, timeout);
        timer_->async_wait(boost::bind(&ConfigHandler::schedule, this, boost::asio::placeholders::error));
    });
}

absl::Status ConfigHandler::parseConfig(ConfigFile& config) {
    std::ifstream ifs {config.filepath, std::ios::in | std::ios::binary};
    if (!ifs) {
        return absl::NotFoundError(absl::StrFormat("error while opening config file: %s, ensure that it exists", config.filepath));
    }

    std::string jsonString;
    std::size_t configFileSize = std::filesystem::file_size(config.filepath);
    jsonString.resize(configFileSize);

    if (static_cast<std::size_t>(ifs.read(jsonString.data(), configFileSize).gcount()) < configFileSize) {
        return absl::InternalError("error while reading file: not all bytes received");
    }

    config.contents = nlohmann::json::parse(jsonString, nullptr, false);
    if (config.contents.is_discarded()) {
        return absl::InternalError("error while parsing json, ensure that config is correct");
    }

    return absl::OkStatus();
}

void ConfigHandler::notify(const Subscriber& sub, const nlohmann::json& config) {
    if (sub.lifetime.lock()) {
        sub.callback((config.contains(sub.section)) ? config[sub.section] : nlohmann::json({}));
    }
}

void ConfigHandler::notifySubscribers(std::vector<Subscriber> subs, ConfigFile& config) {
    for (const auto& sub : subs) {
        notify(sub, config.contents);
    }
}

void ConfigHandler::subscribeOnDefaultConfig(
    const std::string& section, 
    std::function<void(const nlohmann::json&)> callback, 
    std::weak_ptr<void> lifetime) 
{
    std::unique_lock<std::mutex> locked(lock_);
    defaultConfigSubscribers_.emplace_back(Subscriber{
        .section = section,
        .callback = std::move(callback),
        .lifetime = std::move(lifetime)
    });
    notify(defaultConfigSubscribers_.back(), default_.contents);
}

void ConfigHandler::subscribeOnDynamicConfig(
    const std::string& section, 
    std::function<void(const nlohmann::json&)> callback, 
    std::weak_ptr<void> lifetime)
{
    std::unique_lock<std::mutex> locked(lock_);
    dynamicConfigSubscribers_.emplace_back(Subscriber{
        .section = section,
        .callback = std::move(callback),
        .lifetime = std::move(lifetime)
    });
    notify(dynamicConfigSubscribers_.back(), dynamic_.contents);
}