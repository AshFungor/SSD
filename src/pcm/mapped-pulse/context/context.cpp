// // pulse
// #include "protos/server-message.pb.h"
// #include "pulse/def.h"
// #include "pulse/mainloop-api.h"
// #include "pulse/xmalloc.h"
// #include <pulse/context.h>

// // abseil
// #include <absl/strings/str_format.h>

// // laar
// #include <queue>
// #include <src/ssd/macros.hpp>
// #include <src/pcm/mapped-pulse/trace/trace.hpp>

// namespace {

//     void changeContextState(pa_context* c, pa_context_state_t newState) {
//         c->state.state = newState;

//         if (c && c->callbacks.notify.cb) {
//             c->callbacks.notify.cb(c, c->callbacks.notify.userdata);
//         }
//     }

// }

// pa_context *pa_context_new(pa_mainloop_api* m, const char* name) {
//     PCM_STUB();

//     auto context = static_cast<pa_context*>(pa_xmalloc(sizeof(pa_context)));
//     // pa_mainloop_api_once(m, void (*callback)(pa_mainloop_api *, void *), void *userdata)
// }

// void pa_context_unref(pa_context* c) {
//     PCM_STUB();
//     PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(c));
    
//     c->state.refs -= 1;
//     if (c->state.refs < 0) {
//         pa_xfree(c);
//     }
// }

// pa_context* pa_context_ref(pa_context* c) {
//     PCM_STUB();
//     PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(c), nullptr);

//     c->state.refs += 1;
//     return c;
// }

// void pa_context_set_state_callback(pa_context* c, pa_context_notify_cb_t cb, void* userdata) {
//     PCM_STUB();
//     PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(c));

//     c->callbacks.notify.cb = cb;
//     c->callbacks.notify.userdata = userdata;
// }

// int pa_context_errno(const pa_context *c) {
//     PCM_STUB();
//     PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(c), PA_ERR_EXIST);

//     return c->state.error;
// }

// pa_context_state_t pa_context_get_state(const pa_context *c) {
//     PCM_STUB();
//     PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(c));

//     return c->state.state;
// }

// pa_operation* pa_context_exit_daemon(pa_context *c, pa_context_success_cb_t cb, void *userdata) {
//     PCM_MISSED_STUB();
//     UNUSED(c);
//     UNUSED(cb);
//     UNUSED(userdata);

//     return nullptr;
// }

// pa_operation* pa_context_proplist_update(pa_context* c, pa_update_mode_t mode, const pa_proplist* p, pa_context_success_cb_t cb, void* userdata) {
//     PCM_MISSED_STUB();
//     UNUSED(c);
//     UNUSED(mode);
//     UNUSED(p);
//     UNUSED(cb);
//     UNUSED(userdata);

//     return nullptr;
// }

// pa_operation *pa_context_proplist_remove(pa_context *c, const char *const keys[], pa_context_success_cb_t cb, void *userdata) {
//     PCM_MISSED_STUB();
//     UNUSED(c);
//     UNUSED(keys);
//     UNUSED(cb);
//     UNUSED(userdata);

//     return nullptr;
// }