// boost
#include <boost/asio.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

// grpc
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_builder.h>

// Abseil
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/strings/str_cat.h>

// standard
#include <thread>
#include <memory>
#include <optional>

// plog
#include <plog/Log.h>
#include <plog/Severity.h>
#include <plog/Initializers/RollingFileInitializer.h>

// laar
#include <src/ssd/util/config-loader.hpp>
#include <src/ssd/sound/audio-handler.hpp>
#include <src/ssd/core/routing-service.hpp>

ABSL_FLAG(std::optional<std::string>, runtime_dir, std::nullopt, 
    "runtime directory for logs and configs");

int main(int argc, char** argv) {
    absl::ParseCommandLine(argc, argv);

    std::string runtimeDirectory;
    if (std::optional<std::string> flag = absl::GetFlag(FLAGS_runtime_dir); flag.has_value()) {
        runtimeDirectory = flag.value();
        PLOG(plog::info) << "using runtime directory: " << runtimeDirectory;
    } else {
        std::cerr << "no runtime directory provided!";
        return 1;
    }

    auto logDirectory = absl::StrCat(runtimeDirectory, "/server.log");
    plog::init(plog::Severity::debug, logDirectory.c_str(), 1024 * 1024, 2);

    std::size_t threads = std::max<std::size_t>(std::thread::hardware_concurrency() / 2, 1);
    auto context = std::make_shared<boost::asio::io_context>();
    auto tPool = std::make_shared<boost::asio::thread_pool>(threads);

    auto configHandler = laar::ConfigHandler::configure(runtimeDirectory, context);
    PLOG(plog::debug) << "module created: " << "ConfigHandler; instance: " << configHandler.get();

    if (auto result = configHandler->init(); !result.ok()) {
        PLOG(plog::error) << "error: " << result.message();
        return 1;
    }

    auto soundHandler = laar::SoundHandler::configure(configHandler, context);
    soundHandler->init();

    PLOG(plog::debug) << "module created: " << "SoundHandler; instance: " << soundHandler.get();

    auto routingService = laar::RoutingService::configure(soundHandler);
    grpc::ServerBuilder builder;

    builder.AddListeningPort("localhost:7777", grpc::InsecureServerCredentials());
    builder.RegisterService(routingService.get());
    builder.SetSyncServerOption(grpc::ServerBuilder::SyncServerOption::NUM_CQS, 1);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

    if (!server) {
        PLOG(plog::error) << "failed to start server, builder returned nullptr";
        return 1;
    } else {
        PLOG(plog::info) << "server started, blocking main thread";
    }

    for (std::size_t tNum = 0; tNum < threads; ++tNum) {
        boost::asio::post(*tPool, [context]() {
            context->run();
        });
    }

    server->Wait();

    return 0;
}