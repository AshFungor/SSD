// pulse
#include <pulse/def.h>
#include <pulse/cdecl.h>
#include <pulse/stream.h>
#include <pulse/sample.h>
#include <pulse/format.h>
#include <pulse/context.h>
#include <pulse/volume.h>
#include <pulse/context.h>
#include <pulse/xmalloc.h>
#include <pulse/proplist.h>
#include <pulse/operation.h>
#include <pulse/channelmap.h>

// Abseil
#include <absl/strings/str_format.h>

// STD
#include <memory>
#include <cstddef>
#include <cstdint>

// laar
#include <src/ssd/macros.hpp>
#include <src/ssd/core/message.hpp>
#include <src/ssd/sound/converter.hpp>
#include <src/pcm/mapped-pulse/trace/trace.hpp>
#include <src/pcm/mapped-pulse/context/common.hpp>

// proto
#include <protos/holder.pb.h>
#include <protos/client/stream.pb.h>
#include <protos/service/stream.pb.h>
#include <protos/common/stream-configuration.pb.h>

namespace {

    void changeStreamState(pa_stream* s, pa_stream_state_t newState) {
        if (s->state.state == PA_STREAM_FAILED) {
            return;
        }

        s->state.state = newState;
        if (s->callbacks.state.cb) {
            s->callbacks.state.cb(s, s->callbacks.state.userdata);
        }
    }

    void drained(pa_stream* s) {
        if (!--s->state.ops) {
            if (s->state.drain) {
                if (s->callbacks.drain.cb) {
                    s->callbacks.drain.cb(s, 1, s->callbacks.drain.userdata);
                }
                laar::updateOp(s->state.drain, PA_OPERATION_DONE);
                pa_operation_unref(s->state.drain);
            }
        }
    }

    void streamClose(laar::Message message, void* userdata) {
        pa_stream* s = reinterpret_cast<pa_stream*>(userdata);

        if (message.type() != laar::message::type::SIMPLE || laar::messagePayload<laar::message::type::SIMPLE>(message) != laar::ACK) {
            changeStreamState(s, PA_STREAM_FAILED);
        } else {
            changeStreamState(s, PA_STREAM_TERMINATED);
        }

        drained(s);
        pa_stream_unref(s);
    }

    void confirmWrite(laar::Message message, void* userdata) {
        pa_stream* s = reinterpret_cast<pa_stream*>(userdata);

        if (message.type() != laar::message::type::SIMPLE) {
            pcm_log::log("[stream] getting message of wrong type! expecting SIMPLE", pcm_log::ELogVerbosity::ERROR);
            changeStreamState(s, PA_STREAM_FAILED);
        }

        if (auto code = laar::messagePayload<laar::message::type::SIMPLE>(message); code != laar::ACK) {
            pcm_log::log(absl::StrFormat("[stream] getting wrong code, got %d, expected %d", code, laar::ACK), pcm_log::ELogVerbosity::ERROR);
            changeStreamState(s, PA_STREAM_FAILED);
        }
        
        drained(s);
    }

    void cleanup(pa_mainloop_api* a, pa_stream* s, pa_time_event* e) {
        changeStreamState(s, PA_STREAM_FAILED);
        a->time_free(e);
    }

    void queryStream(pa_mainloop_api* a, pa_time_event* e, const struct timeval* tv, void* userdata) {
        // tv might be dead by the time callback is invoked
        UNUSED(tv);
        pa_stream* s = reinterpret_cast<pa_stream*>(userdata);

        // only playback is supported
        if (s->pulseAttributes.dir != PA_STREAM_PLAYBACK) {
            a->time_free(e);
            return;
        }

        // cork is serving its purpose
        if (s->state.cork) {
            pcm_log::log("[stream] stream corked, missing cycle", pcm_log::ELogVerbosity::INFO);
            a->time_free(e);
            return;
        }

        s->buffer.avail += laar::SamplesPerTimeFrame * laar::getSampleSize(s->network.config.sample_spec().format());;
        if (s->buffer.avail > s->buffer.size) {
            pcm_log::log("[stream] avail reached the size of buffer, aborting stream", pcm_log::ELogVerbosity::ERROR);
            cleanup(a, s, e);
            return;
        }

        if (s->callbacks.write.cb) {
            s->callbacks.write.cb(s, s->buffer.avail - s->buffer.wPos, s->callbacks.write.userdata);
        }

        pa_context_rttime_restart(s->state.context, e, laar::TimeFrame.count());
    }

    void confirmStreamOpen(laar::Message message, void* userdata) {
        pa_stream* s = reinterpret_cast<pa_stream*>(userdata);

        NSound::THolder holder = laar::messagePayload<laar::message::type::PROTOBUF>(message);

        s->network.id = holder.server().stream_message().stream_id();
        s->network.config = std::move(*holder.mutable_server()->mutable_stream_message()->mutable_connect_confirmal()->mutable_configuration());

        if (holder.server().stream_message().connect_confirmal().opened()) {
            changeStreamState(s, PA_STREAM_READY);
        } else {
            changeStreamState(s, PA_STREAM_FAILED);
        }

        drained(s);

        if (s->callbacks.start.cb) {
            s->callbacks.start.cb(s, s->callbacks.start.userdata);
        }

        // set up timer event for data writing
        pa_context_rttime_new(s->state.context, laar::TimeFrame.count() * 1000, queryStream, s);
    }

    void openStream(pa_stream *s, pa_stream_direction dir, const char* name, const pa_buffer_attr* attr) {
        NSound::NCommon::TStreamConfiguration config;

        config.set_client_name("laar-slave");
        config.set_stream_name(name);
        config.set_direction(NSound::NCommon::TStreamConfiguration::PLAYBACK);
        s->pulseAttributes.dir = dir;

        if (dir == PA_STREAM_PLAYBACK) {
            config.set_direction(NSound::NCommon::TStreamConfiguration::PLAYBACK);
        } else if (dir == PA_STREAM_RECORD) {
            config.set_direction(NSound::NCommon::TStreamConfiguration::RECORD);
        } else {
            pcm_log::log(absl::StrFormat("[stream] unsupported stream direction: %d", dir), pcm_log::ELogVerbosity::ERROR);
        }

        if (auto ss = &s->pulseAttributes.spec) {

            if (ss->rate == laar::BaseSampleRate) {
                config.mutable_sample_spec()->set_sample_rate(laar::BaseSampleRate);
            } else {
                pcm_log::log(absl::StrFormat("[stream] unsupported rate: %d", ss->rate), pcm_log::ELogVerbosity::ERROR);
            }

            if (ss->format == PA_SAMPLE_U8) {
                config.mutable_sample_spec()->set_format(NSound::NCommon::TStreamConfiguration::TSampleSpecification::UNSIGNED_8);
            } else if (ss->format == PA_SAMPLE_S16LE) {
                config.mutable_sample_spec()->set_format(NSound::NCommon::TStreamConfiguration::TSampleSpecification::SIGNED_16_LITTLE_ENDIAN);
            } else if (ss->format == PA_SAMPLE_S16BE) {
                config.mutable_sample_spec()->set_format(NSound::NCommon::TStreamConfiguration::TSampleSpecification::SIGNED_16_BIG_ENDIAN);
            } else if (ss->format == PA_SAMPLE_S24LE) {
                config.mutable_sample_spec()->set_format(NSound::NCommon::TStreamConfiguration::TSampleSpecification::SIGNED_24_LITTLE_ENDIAN);
            } else if (ss->format == PA_SAMPLE_S24BE) {
                config.mutable_sample_spec()->set_format(NSound::NCommon::TStreamConfiguration::TSampleSpecification::SIGNED_24_BIG_ENDIAN);
            } else if (ss->format == PA_SAMPLE_S32LE) {
                config.mutable_sample_spec()->set_format(NSound::NCommon::TStreamConfiguration::TSampleSpecification::SIGNED_32_LITTLE_ENDIAN);
            } else if (ss->format == PA_SAMPLE_S32BE) {
                config.mutable_sample_spec()->set_format(NSound::NCommon::TStreamConfiguration::TSampleSpecification::SIGNED_32_BIG_ENDIAN);
            } else if (ss->format == PA_SAMPLE_FLOAT32LE) {
                config.mutable_sample_spec()->set_format(NSound::NCommon::TStreamConfiguration::TSampleSpecification::FLOAT_32_LITTLE_ENDIAN);
            } else if (ss->format == PA_SAMPLE_FLOAT32BE) {
                config.mutable_sample_spec()->set_format(NSound::NCommon::TStreamConfiguration::TSampleSpecification::FLOAT_32_BIG_ENDIAN);
            } else {
                pcm_log::log(absl::StrFormat("[stream] unsupported format: %d", ss->format), pcm_log::ELogVerbosity::ERROR);
            }

            if (ss->channels == 1) {
                config.mutable_sample_spec()->set_channels(ss->channels);
            } else {
                pcm_log::log(absl::StrFormat("[stream] unsupported channel number: %d", ss->channels), pcm_log::ELogVerbosity::ERROR);
            }
        }

        if (attr) {
            s->pulseAttributes.buffer = *attr;
            config.mutable_buffer_config()->set_prebuffing_size(attr->prebuf);
            config.mutable_buffer_config()->set_fragment_size(attr->fragsize);
            config.mutable_buffer_config()->set_min_request_size(attr->minreq);
            config.mutable_buffer_config()->set_size(attr->tlength);
        }

        NSound::NClient::TStreamMessage streamMessage;
        streamMessage.set_stream_id(UINT32_MAX);
        *streamMessage.mutable_connect()->mutable_configuration() = std::move(config);

        NSound::THolder holder;
        *holder.mutable_client()->mutable_stream_message() = std::move(streamMessage);

        pa_operation* o = new pa_operation;
        o->cbSuccess = confirmStreamOpen;
        o->owner = s;
        o->cbNotify = nullptr;
        o->userdata = nullptr;
        o->refs = 0;
        o->state = PA_OPERATION_RUNNING;

        ++s->state.ops;
        pa_operation_ref(o);

        auto message = s->state.context->network.factory->withType(laar::message::type::PROTOBUF)
            .withPayload(std::move(holder))
            .construct()
            .constructed();

        s->state.context->out.push_back(pa_context::QueuedMessage{
            .message = std::move(message),
            .op = o
        });

        changeStreamState(s, PA_STREAM_CREATING);
    }

}

pa_stream* pa_stream_new(pa_context *c, const char* name, const pa_sample_spec* ss, const pa_channel_map* map) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(c), nullptr);
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(name), nullptr);
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(map), nullptr);
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(ss), nullptr);

    auto s = static_cast<pa_stream*>(pa_xmalloc(sizeof(pa_stream)));
    std::construct_at(s);

    s->buffer.buffer = std::make_unique<std::uint8_t[]>(laar::NetworkBufferSize);
    s->buffer.size = laar::NetworkBufferSize;
    s->buffer.avail = s->buffer.rPos = s->buffer.wPos = 0;
    s->buffer.directWrite = false;
    
    s->callbacks.write.cb = nullptr;
    s->callbacks.write.userdata = nullptr;
    s->callbacks.read.cb = nullptr;
    s->callbacks.read.userdata = nullptr;
    s->callbacks.drain.cb = nullptr;
    s->callbacks.drain.userdata = nullptr;
    s->callbacks.start.cb = nullptr;
    s->callbacks.start.userdata = nullptr;
    s->callbacks.state.cb = nullptr;
    s->callbacks.state.userdata = nullptr;

    s->state.context = c;
    s->state.state = PA_STREAM_UNCONNECTED;
    s->state.cork = false;
    s->state.drain = nullptr;
    s->state.ops = 0;
    s->state.refs = 0;
    
    s->pulseAttributes.map = *map;
    s->pulseAttributes.spec = *ss;
    s->pulseAttributes.name = name;
    
    s->network.id = UINT32_MAX;

    pa_stream_ref(s);

    return s;
}

pa_stream* pa_stream_new_with_proplist(pa_context* c, const char* name, const pa_sample_spec* ss, const pa_channel_map* map, pa_proplist* p) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(c), nullptr);
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(name), nullptr);
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(map), nullptr);
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(ss), nullptr);
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(p), nullptr);

    return pa_stream_new(c, name, ss, map);
}

pa_stream* pa_stream_new_extended(pa_context* c, const char* name, pa_format_info* const* formats, unsigned int n_formats, pa_proplist* p) {
    PCM_MISSED_STUB();
    UNUSED(c);
    UNUSED(name);
    UNUSED(formats);
    UNUSED(n_formats);
    UNUSED(p);

    return nullptr;
}

void pa_stream_unref(pa_stream* s) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(s));

    --s->state.refs;
    if (s->state.refs <= 0) {
        std::destroy_at(s);
        pa_xfree(s);
    }
}

pa_stream* pa_stream_ref(pa_stream* s) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(s));

    ++s->state.refs;
    return s;
}

pa_stream_state_t pa_stream_get_state(const pa_stream* p) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(p));

    return p->state.state;
}

pa_context* pa_stream_get_context(const pa_stream* p) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(p));

    return p->state.context;
}

uint32_t pa_stream_get_index(const pa_stream* s) {
    PCM_MISSED_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(s));

    // maybe sometime later
    return 0;
}

uint32_t pa_stream_get_device_index(const pa_stream* s) {
    PCM_MISSED_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(s));

    return PA_INVALID_INDEX;
}

const char* pa_stream_get_device_name(const pa_stream* s) {
    PCM_MISSED_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(s));

    return "default-laar-sink";
}

int pa_stream_is_suspended(const pa_stream *s) {
    PCM_MISSED_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(s));

    return PA_ERR_NOTSUPPORTED;
}

int pa_stream_is_corked(const pa_stream *s) {
    PCM_MISSED_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(s));

    // we may even support this :)
    return 0;
}

int pa_stream_connect_playback(pa_stream* s, const char* dev, const pa_buffer_attr* attr, pa_stream_flags_t flags, const pa_cvolume* volume, pa_stream* sync_stream) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(s), PA_ERR_EXIST);
    UNUSED(dev);
    UNUSED(flags);
    UNUSED(volume);
    UNUSED(sync_stream);

    openStream(s, PA_STREAM_PLAYBACK, s->pulseAttributes.name.c_str(), attr);

    return PA_OK;
}

int pa_stream_connect_record(pa_stream *s, const char *dev, const pa_buffer_attr *attr, pa_stream_flags_t flags) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(s), PA_ERR_EXIST);
    UNUSED(dev);
    UNUSED(flags);
    UNUSED(attr);

    openStream(s, PA_STREAM_RECORD, s->pulseAttributes.name.c_str(), attr);

    return PA_OK;
}

int pa_stream_disconnect(pa_stream* s) {
    PCM_STUB();
    UNUSED(s);

    NSound::THolder holder;
    holder.mutable_client()->mutable_stream_message()->set_stream_id(s->network.id);
    *holder.mutable_client()->mutable_stream_message()->mutable_close() = NSound::NClient::TStreamMessage::TClose::default_instance();

    s->state.cork = true;

    pa_operation* o = new pa_operation;
    o->cbSuccess = streamClose;
    o->owner = s;
    o->cbNotify = nullptr;
    o->userdata = nullptr;
    o->refs = 0;
    o->state = PA_OPERATION_RUNNING;

    ++s->state.ops;
    pa_operation_ref(o);

    auto message = s->state.context->network.factory->withType(laar::message::type::PROTOBUF)
        .withPayload(std::move(holder))
        .construct()
        .constructed();

    s->state.context->out.push_back(pa_context::QueuedMessage{
        .message = std::move(message),
        .op = o
    });

    return PA_OK;
}

int pa_stream_begin_write(pa_stream* p, void** data, size_t* nbytes) {
    PCM_STUB();

    *data = p->buffer.buffer.get() + p->buffer.wPos;
    *nbytes = pa_stream_writable_size(p);

    p->buffer.directWrite = true;
    return PA_OK;
}

int pa_stream_cancel_write(pa_stream* p) {
    PCM_STUB();
    UNUSED(p);

    p->buffer.directWrite = false;
    return PA_OK;
}

int pa_stream_write(pa_stream* p, const void* data, size_t nbytes, pa_free_cb_t free_cb, int64_t offset, pa_seek_mode_t seek) {
    PCM_STUB();

    return pa_stream_write_ext_free(p, data, nbytes, free_cb, const_cast<void*>(data), offset, seek);
}

int pa_stream_write_ext_free(pa_stream* p, const void* data, size_t nbytes, pa_free_cb_t free_cb, void* free_cb_data, int64_t offset, pa_seek_mode_t seek) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(p), PA_ERR_EXIST);
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(data), PA_ERR_EXIST);

    std::size_t currWPos = offset;
    switch (seek) {
        case PA_SEEK_RELATIVE:
        case PA_SEEK_RELATIVE_END:
            currWPos += p->buffer.wPos;
            break;
        case PA_SEEK_RELATIVE_ON_READ:
            currWPos += p->buffer.rPos;
            break;
        case PA_SEEK_ABSOLUTE:
            currWPos += 0;
            break;
    }

    pcm_log::log(absl::StrFormat("[stream] calling write with seek mode: %d, offset: %d", seek, offset), pcm_log::ELogVerbosity::INFO);

    if (!p->buffer.directWrite) {
        pcm_log::log(absl::StrFormat("[stream] detecting an indirect write; writing %d bytes at pos: %d", nbytes, currWPos), pcm_log::ELogVerbosity::INFO);
        
        if (nbytes + currWPos > p->buffer.size) {
            pcm_log::log("[stream] writing too much! returning early", pcm_log::ELogVerbosity::ERROR);
            return PA_ERR_TOOLARGE;
        }

        std::memcpy(p->buffer.buffer.get() + currWPos, data, nbytes);
        pcm_log::log("[stream] writing ended, assembling call", pcm_log::ELogVerbosity::INFO);
    } else {
        pcm_log::log(absl::StrFormat("[stream] direct write, assuming %d bytes already in buffer", nbytes), pcm_log::ELogVerbosity::INFO);
    }

    // prepare for tiling data
    std::size_t tileSize = pa_context_get_tile_size(p->state.context, &p->pulseAttributes.spec);
    auto temp = std::make_unique<std::uint8_t[]>(tileSize);

    for (; p->buffer.rPos < p->buffer.avail; p->buffer.rPos += tileSize) {
        pcm_log::log(absl::StrFormat("[stream] assembling tile, rpos: %d, wpos: %d, avail: %d", p->buffer.rPos, p->buffer.wPos, p->buffer.avail), pcm_log::ELogVerbosity::INFO);
        std::size_t end = std::min(p->buffer.rPos + tileSize, p->buffer.avail);
        std::memcpy(temp.get(), p->buffer.buffer.get() + p->buffer.rPos, end - p->buffer.rPos);

        NSound::THolder holder;
        NSound::NClient::TStreamMessage streamMessage;
        streamMessage.set_stream_id(p->network.id);
        streamMessage.mutable_push()->set_data(std::string{reinterpret_cast<char*>(temp.get()), tileSize});
        streamMessage.mutable_push()->set_size(tileSize / laar::getSampleSize(p->network.config.sample_spec().format()));

        *holder.mutable_client()->mutable_stream_message() = std::move(streamMessage);
        auto msg = p->state.context->network.factory->withType(laar::message::type::PROTOBUF)
            .withPayload(std::move(holder))
            .construct()
            .constructed();

        pa_operation* o = new pa_operation;
        o->cbNotify = nullptr;
        o->cbSuccess = confirmWrite;
        o->owner = p;
        o->refs = 0;
        o->state = PA_OPERATION_RUNNING;
        o->userdata = nullptr;

        ++p->state.ops;
        pa_operation_ref(o);

        p->state.context->out.push_back(pa_context::QueuedMessage{
            .message = std::move(msg),
            .op = o
        });
    }


    // if relative, update internal wPos
    if (seek == PA_SEEK_RELATIVE && !offset) {
        p->buffer.wPos += nbytes;

        ENSURE_FAIL_UNLESS(p->buffer.rPos == p->buffer.wPos);
        ENSURE_FAIL_UNLESS(p->buffer.wPos == p->buffer.avail);

        p->buffer.rPos = p->buffer.wPos = p->buffer.avail = 0;
    }

    if (free_cb) {
        free_cb(free_cb_data);
    }

    p->buffer.directWrite = false;
    return PA_OK;
}


int pa_stream_peek(pa_stream* p, const void** data, size_t *nbytes) {
    PCM_MISSED_STUB();
    UNUSED(p);
    UNUSED(data);
    UNUSED(nbytes);

    return PA_ERR_NOTSUPPORTED;
}

int pa_stream_drop(pa_stream* p) {
    PCM_MISSED_STUB();
    UNUSED(p);

    return PA_ERR_NOTSUPPORTED;
}

size_t pa_stream_writable_size(const pa_stream* p) {
    PCM_STUB();

    return p->buffer.avail - p->buffer.wPos;
}

size_t pa_stream_readable_size(const pa_stream* p) {
    PCM_MISSED_STUB();
    UNUSED(p);

    return 0;
}

pa_operation* pa_stream_drain(pa_stream* s, pa_stream_success_cb_t cb, void* userdata) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(s), nullptr);

    pa_operation* o = new pa_operation;
    o->cbNotify = nullptr;
    o->cbSuccess = nullptr;
    o->owner = nullptr;
    o->state = PA_OPERATION_RUNNING;
    o->userdata = nullptr;
    o->refs = 0;

    pa_operation_ref(o);

    s->callbacks.drain.cb = cb;
    s->callbacks.drain.userdata = userdata;
    s->state.drain = o;

    return o;
}

pa_operation* pa_stream_update_timing_info(pa_stream* p, pa_stream_success_cb_t cb, void* userdata) {
    PCM_MISSED_STUB();
    UNUSED(p);
    UNUSED(cb);
    UNUSED(userdata);

    return nullptr;
}

void pa_stream_set_state_callback(pa_stream* s, pa_stream_notify_cb_t cb, void* userdata) {
    PCM_STUB();

    s->callbacks.state.cb = cb;
    s->callbacks.state.userdata = userdata;
}

void pa_stream_set_write_callback(pa_stream* s, pa_stream_request_cb_t cb, void* userdata) {
    PCM_STUB();

    s->callbacks.write.cb = cb;
    s->callbacks.write.userdata = userdata;
}

void pa_stream_set_read_callback(pa_stream* s, pa_stream_request_cb_t cb, void* userdata) {
    PCM_STUB();

    s->callbacks.read.cb = cb;
    s->callbacks.read.userdata = userdata;
}

void pa_stream_set_overflow_callback(pa_stream* p, pa_stream_notify_cb_t cb, void* userdata) {
    PCM_MISSED_STUB();
    UNUSED(p);
    UNUSED(cb);
    UNUSED(userdata);
}

int64_t pa_stream_get_underflow_index(const pa_stream* p) {
    PCM_MISSED_STUB();
    UNUSED(p);

    return -1;
}

void pa_stream_set_underflow_callback(pa_stream* p, pa_stream_notify_cb_t cb, void* userdata) {
    PCM_MISSED_STUB();
    UNUSED(p);
    UNUSED(cb);
    UNUSED(userdata);
}

void pa_stream_set_started_callback(pa_stream* s, pa_stream_notify_cb_t cb, void* userdata) {
    PCM_STUB();

    s->callbacks.start.cb = cb;
    s->callbacks.start.userdata = userdata;
}

void pa_stream_set_latency_update_callback(pa_stream* p, pa_stream_notify_cb_t cb, void* userdata) {
    PCM_MISSED_STUB();
    UNUSED(p);
    UNUSED(cb);
    UNUSED(userdata);
}

void pa_stream_set_moved_callback(pa_stream* p, pa_stream_notify_cb_t cb, void* userdata) {
    PCM_MISSED_STUB();
    UNUSED(p);
    UNUSED(cb);
    UNUSED(userdata);
}

void pa_stream_set_suspended_callback(pa_stream* p, pa_stream_notify_cb_t cb, void* userdata) {
    PCM_MISSED_STUB();
    UNUSED(p);
    UNUSED(cb);
    UNUSED(userdata);
}

void pa_stream_set_event_callback(pa_stream* p, pa_stream_event_cb_t cb, void* userdata) {
    PCM_MISSED_STUB();
    UNUSED(p);
    UNUSED(cb);
    UNUSED(userdata);
}

void pa_stream_set_buffer_attr_callback(pa_stream* p, pa_stream_notify_cb_t cb, void* userdata) {
    PCM_MISSED_STUB();
    UNUSED(p);
    UNUSED(cb);
    UNUSED(userdata);
}

pa_operation* pa_stream_cork(pa_stream* s, int b, pa_stream_success_cb_t cb, void* userdata) {
    PCM_MISSED_STUB();
    UNUSED(s);
    UNUSED(b);
    UNUSED(cb);
    UNUSED(userdata);

    return nullptr;
}

pa_operation* pa_stream_flush(pa_stream* s, pa_stream_success_cb_t cb, void* userdata) {
    PCM_MISSED_STUB();
    UNUSED(s);
    UNUSED(cb);
    UNUSED(userdata);

    return nullptr;
}

pa_operation* pa_stream_prebuf(pa_stream* s, pa_stream_success_cb_t cb, void* userdata) {
    PCM_MISSED_STUB();
    UNUSED(s);
    UNUSED(cb);
    UNUSED(userdata);

    return nullptr;
}

pa_operation* pa_stream_trigger(pa_stream* s, pa_stream_success_cb_t cb, void* userdata) {
    PCM_MISSED_STUB();
    UNUSED(s);
    UNUSED(cb);
    UNUSED(userdata);

    return nullptr;
}

pa_operation* pa_stream_set_name(pa_stream* s, const char* name, pa_stream_success_cb_t cb, void* userdata) {
    PCM_MISSED_STUB();
    UNUSED(s);
    UNUSED(name);
    UNUSED(cb);
    UNUSED(userdata);

    return nullptr;
}

int pa_stream_get_time(pa_stream* s, pa_usec_t* r_usec) {
    PCM_MISSED_STUB();
    UNUSED(r_usec);
    UNUSED(s);

    return PA_ERR_NOTSUPPORTED;
}

int pa_stream_get_latency(pa_stream* s, pa_usec_t* r_usec, int* negative) {
    PCM_MISSED_STUB();
    UNUSED(negative);
    UNUSED(r_usec);
    UNUSED(s);

    return PA_ERR_NOTSUPPORTED;
}

const pa_timing_info* pa_stream_get_timing_info(pa_stream* s) {
    PCM_MISSED_STUB();
    UNUSED(s);

    return nullptr;
}

const pa_sample_spec* pa_stream_get_sample_spec(pa_stream* s) {
    PCM_STUB();

    return &s->pulseAttributes.spec;
}

const pa_channel_map* pa_stream_get_channel_map(pa_stream* s) {
    PCM_STUB();

    return &s->pulseAttributes.map;
}

const pa_format_info* pa_stream_get_format_info(const pa_stream* s) {
    PCM_MISSED_STUB();
    UNUSED(s);

    return nullptr;
}

const pa_buffer_attr* pa_stream_get_buffer_attr(pa_stream* s) {
    PCM_STUB();

    return &s->pulseAttributes.buffer;
}

pa_operation* pa_stream_set_buffer_attr(pa_stream* s, const pa_buffer_attr* attr, pa_stream_success_cb_t cb, void* userdata) {
    PCM_MISSED_STUB();
    UNUSED(s);
    UNUSED(cb);
    UNUSED(attr);
    UNUSED(userdata);

    return nullptr;
}

pa_operation* pa_stream_update_sample_rate(pa_stream* s, uint32_t rate, pa_stream_success_cb_t cb, void* userdata) {
    PCM_MISSED_STUB();
    UNUSED(s);
    UNUSED(cb);
    UNUSED(rate);
    UNUSED(userdata);

    return nullptr;
}

pa_operation* pa_stream_proplist_update(pa_stream* s, pa_update_mode_t mode, pa_proplist* p, pa_stream_success_cb_t cb, void* userdata) {
    PCM_MISSED_STUB();
    UNUSED(s);
    UNUSED(mode);
    UNUSED(p);
    UNUSED(cb);
    UNUSED(userdata);

    return nullptr;
}

pa_operation* pa_stream_proplist_remove(pa_stream* s, const char* const keys[], pa_stream_success_cb_t cb, void* userdata) {
    PCM_MISSED_STUB();
    UNUSED(s);
    UNUSED(keys);
    UNUSED(cb);
    UNUSED(userdata);

    return nullptr;
}

int pa_stream_set_monitor_stream(pa_stream* s, uint32_t sink_input_idx) {
    PCM_MISSED_STUB();
    UNUSED(s);
    UNUSED(sink_input_idx);

    return PA_ERR_NOTSUPPORTED;
}

uint32_t pa_stream_get_monitor_stream(const pa_stream* s) {
    PCM_MISSED_STUB();
    UNUSED(s);

    return PA_INVALID_INDEX;
}
