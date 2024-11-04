// laar
#include <src/ssd/macros.hpp>
#include <src/ssd/core/server.hpp>
#include <src/ssd/core/message.hpp>
#include <src/ssd/core/session/stream.hpp>
#include <src/ssd/core/interfaces/i-stream.hpp>
#include <src/ssd/core/interfaces/i-context.hpp>
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>

// plog
#include <plog/Log.h>
#include <plog/Severity.h>

// boost
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/system_error.hpp>
#include <boost/system/detail/error_code.hpp>

// protos
#include <protos/holder.pb.h>
#include <protos/client/stream.pb.h>
#include <protos/common/directives.pb.h>
#include <protos/common/stream-configuration.pb.h>

// Abseil
#include <absl/status/status.h>

// STD
#include <memory>

using namespace laar;

namespace {

    std::string makeUnimplementedMessage(std::source_location location = std::source_location::current()) {
        return absl::StrFormat(
            "Unimplemented API; File: \"%s\"; Function: \"%s\"; Line: %d", location.file_name(), location.function_name(), location.line()
        );
    }

    std::string dumpStreamConfig(const NSound::NCommon::TStreamConfiguration& message) {
        std::stringstream ss;
        ss << "Message parsed: NSound::NCommon::TStreamConfiguration\n";
        if (message.has_buffer_config()) {
            const auto& msg = message.buffer_config();
            ss << "- TBufferConfig: present\n";
            ss << "  size: " << msg.size() << "\n";
            ss << "  prebuffing_size: " << msg.prebuffing_size() << "\n";
            ss << "  min_request_size: " << msg.min_request_size() << "\n";
            ss << "  fragment_size: " << msg.fragment_size() << "\n";
        } else {
            ss << "- TBufferConfig: missing\n";
        }
        if (message.has_channel_map()) {
            ss << "- TChannelMap: present\n";
            for (const auto& channel : message.channel_map().mapped_channel()) {
                ss << "  Channel value:\n";
                if (channel.has_position()) {
                    ss << "  Position: " << channel.position() << "\n";
                }
                if (channel.has_aux()) {
                    ss << "  AUX: " << channel.aux() << "\n"; 
                }
            }
        } else {
            ss << "- TChannelMap: missing\n";
        }
        if (message.has_client_name()) {
            ss << "- ClientName: " << message.client_name() << "\n";
        } else {
            ss << "- ClientName: missing\n"; 
        }
        if (message.has_stream_name()) {
            ss << "- StreamName: " << message.stream_name() << "\n";
        } else {
            ss << "- StreamName: missing\n"; 
        }
        if (message.has_sample_spec()) {
            const auto& msg = message.sample_spec();
            ss << "- TSampleSpec: present\n";
            ss << "  Format: " << msg.format() << "\n";
            ss << "  SampleRate: " << msg.sample_rate() << "\n";
            ss << "  Channels: " << msg.channels() << "\n";
        } else {
            ss << "- TSampleSpec: missing\n";
        }
        ss << "- Direction: " << message.direction() << "\n";
        ss << "Parsing ended.";
        return ss.str();
    }

}

std::shared_ptr<Stream> Stream::configure(
    std::shared_ptr<boost::asio::io_context> context,
    std::weak_ptr<IStream::IStreamMaster> master, 
    std::weak_ptr<IStreamHandler> handler
) {
    return std::shared_ptr<Stream>(new Stream{std::move(context), std::move(master), std::move(handler)});
}

Stream::Stream(
    std::shared_ptr<boost::asio::io_context> context,
    std::weak_ptr<IStream::IStreamMaster> master, 
    std::weak_ptr<IStreamHandler> handler
)
    : context_(std::move(context))
    , handler_(std::move(handler))
    , master_(std::move(master))
{}

void Stream::init() {
    // do nothing
}

Stream::operator bool() const {
    return true;
}

IContext::APIResult Stream::onClientMessage(NSound::NClient::TStreamMessage message) {
    PLOG(plog::debug) << "[stream] Handling API on stream";

    if (message.has_close()) {
        return onClose(std::move(*message.mutable_close()));
    } else if (message.has_connect()) {
        return onStreamConfiguration(std::move(*message.mutable_connect()->mutable_configuration()));
    } else if (message.has_push()) {
        return onIOOperation(std::move(*message.mutable_push()));
    }

    return IContext::APIResult::unimplemented();
}

IContext::APIResult Stream::onIOOperation(NSound::NClient::TStreamMessage::TPull message) {
    UNUSED(message);

    return IContext::APIResult::unimplemented();
}

IContext::APIResult Stream::onIOOperation(NSound::NClient::TStreamMessage::TPush message) {
    PLOG(plog::debug) << "[stream] receiving data";

    if (streamConfig_->direction() == NSound::NCommon::TStreamConfiguration::PLAYBACK) {
        auto status = handle_->write(message.data().c_str(), message.size());
        PLOG(plog::debug) << "write status: " << status.status().ToString();
        return IContext::APIResult{absl::OkStatus()};
    } else if (streamConfig_->direction() == NSound::NCommon::TStreamConfiguration::RECORD) {
        return IContext::APIResult::misconfiguration("received wrong IO operation: write");
    }

    return IContext::APIResult::unimplemented(makeUnimplementedMessage());
}

IContext::APIResult Stream::onStreamConfiguration(NSound::NCommon::TStreamConfiguration message) {
    PLOG(plog::debug) << "[stream] connecting client stream";
    if (streamConfig_.has_value()) {
        return IContext::APIResult{absl::InternalError("double config on stream")};
    }

    if (auto handler = handler_.lock(); handler) {
        switch (message.direction()) {
            case NSound::NCommon::TStreamConfiguration::PLAYBACK:
                handle_ = handler->acquireWriteHandle(message, weak_from_this());
                break;
            case NSound::NCommon::TStreamConfiguration::RECORD:
                handle_ = handler->acquireReadHandle(message, weak_from_this());
                break;
            default:
                return IContext::APIResult::unimplemented(makeUnimplementedMessage());
        }
    }

    PLOG(plog::debug) << dumpStreamConfig(message);

    streamConfig_ = std::move(message);
    NSound::THolder holder;
    holder.mutable_server()->mutable_stream_message()->mutable_connect_confirmal()->mutable_configuration()->CopyFrom(streamConfig_.value());
    holder.mutable_server()->mutable_stream_message()->mutable_connect_confirmal()->set_opened(true);
    // FIXME: add stream id in patch
    holder.mutable_server()->mutable_stream_message()->set_stream_id(0);
    return IContext::APIResult{absl::OkStatus(), std::move(holder)};
}

IContext::APIResult Stream::onDrain(NSound::NCommon::TStreamDirective message) {
    UNUSED(message);

    return IContext::APIResult::unimplemented(makeUnimplementedMessage());
}

IContext::APIResult Stream::onFlush(NSound::NCommon::TStreamDirective message) {
    UNUSED(message);

    return IContext::APIResult::unimplemented(makeUnimplementedMessage());
}

IContext::APIResult Stream::onClose(NSound::NClient::TStreamMessage::TClose message) {
    UNUSED(message);

    PLOG(plog::debug) << "[stream] closing stream";
    if (auto master = master_.lock()) {
        master->wrapClose(context_, weak_from_this());
    }
    return IContext::APIResult{absl::OkStatus()};
}

void Stream::onBufferDrained(int status) {
    UNUSED(status);
}

void Stream::onBufferFlushed(int status) {
    UNUSED(status);
}