// pulse
#include <pulse/operation.h>

// laar
#include <src/ssd/macros.hpp>
#include <src/pcm/mapped-pulse/context/common.hpp>

pa_operation* pa_operation_ref(pa_operation* o) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(o), nullptr);

    ++o->refs;
    return o;
}

void pa_operation_unref(pa_operation* o) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(o));

    --o->refs;
    if (o->refs <= 0) {
        delete o;
    }
}

void pa_operation_cancel(pa_operation* o) {
    PCM_MISSED_STUB();
    UNUSED(o);
}

pa_operation_state_t pa_operation_get_state(const pa_operation* o) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(o));

    return o->state;
}

void pa_operation_set_state_callback(pa_operation *o, pa_operation_notify_cb_t cb, void* userdata) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(o));

    o->cbNotify = cb;
    o->userdata = userdata;

    laar::updateOp(o, o->state);
}