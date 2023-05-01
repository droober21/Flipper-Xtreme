#include "base.h"
#include "check.h"
#include "stream_buffer.h"
#include "common_defines.h"
#include <FreeRTOS.h>
#include <FreeRTOS-Kernel/include/stream_buffer.h>

FurryStreamBuffer* furry_stream_buffer_alloc(size_t size, size_t trigger_level) {
    furry_assert(size != 0);

    StreamBufferHandle_t handle = xStreamBufferCreate(size, trigger_level);
    furry_check(handle);

    return handle;
};

void furry_stream_buffer_free(FurryStreamBuffer* stream_buffer) {
    furry_assert(stream_buffer);
    vStreamBufferDelete(stream_buffer);
};

bool furry_stream_set_trigger_level(FurryStreamBuffer* stream_buffer, size_t trigger_level) {
    furry_assert(stream_buffer);
    return xStreamBufferSetTriggerLevel(stream_buffer, trigger_level) == pdTRUE;
};

size_t furry_stream_buffer_send(
    FurryStreamBuffer* stream_buffer,
    const void* data,
    size_t length,
    uint32_t timeout) {
    size_t ret;

    if(FURRY_IS_IRQ_MODE()) {
        BaseType_t yield;
        ret = xStreamBufferSendFromISR(stream_buffer, data, length, &yield);
        portYIELD_FROM_ISR(yield);
    } else {
        ret = xStreamBufferSend(stream_buffer, data, length, timeout);
    }

    return ret;
};

size_t furry_stream_buffer_receive(
    FurryStreamBuffer* stream_buffer,
    void* data,
    size_t length,
    uint32_t timeout) {
    size_t ret;

    if(FURRY_IS_IRQ_MODE()) {
        BaseType_t yield;
        ret = xStreamBufferReceiveFromISR(stream_buffer, data, length, &yield);
        portYIELD_FROM_ISR(yield);
    } else {
        ret = xStreamBufferReceive(stream_buffer, data, length, timeout);
    }

    return ret;
}

size_t furry_stream_buffer_bytes_available(FurryStreamBuffer* stream_buffer) {
    return xStreamBufferBytesAvailable(stream_buffer);
};

size_t furry_stream_buffer_spaces_available(FurryStreamBuffer* stream_buffer) {
    return xStreamBufferSpacesAvailable(stream_buffer);
};

bool furry_stream_buffer_is_full(FurryStreamBuffer* stream_buffer) {
    return xStreamBufferIsFull(stream_buffer) == pdTRUE;
};

bool furry_stream_buffer_is_empty(FurryStreamBuffer* stream_buffer) {
    return (xStreamBufferIsEmpty(stream_buffer) == pdTRUE);
};

FurryStatus furry_stream_buffer_reset(FurryStreamBuffer* stream_buffer) {
    if(xStreamBufferReset(stream_buffer) == pdPASS) {
        return FurryStatusOk;
    } else {
        return FurryStatusError;
    }
}