#include "stream.h"
#include "stream_i.h"
#include "string_stream.h"
#include <core/common_defines.h>

typedef struct {
    Stream stream_base;
    FurryString* string;
    size_t index;
} StringStream;

static size_t string_stream_write_char(StringStream* stream, char c);

static void string_stream_free(StringStream* stream);
static bool string_stream_eof(StringStream* stream);
static void string_stream_clean(StringStream* stream);
static bool string_stream_seek(StringStream* stream, int32_t offset, StreamOffset offset_type);
static size_t string_stream_tell(StringStream* stream);
static size_t string_stream_size(StringStream* stream);
static size_t string_stream_write(StringStream* stream, const char* data, size_t size);
static size_t string_stream_read(StringStream* stream, char* data, size_t size);
static bool string_stream_delete_and_insert(
    StringStream* stream,
    size_t delete_size,
    StreamWriteCB write_callback,
    const void* ctx);

const StreamVTable string_stream_vtable = {
    .free = (StreamFreeFn)string_stream_free,
    .eof = (StreamEOFFn)string_stream_eof,
    .clean = (StreamCleanFn)string_stream_clean,
    .seek = (StreamSeekFn)string_stream_seek,
    .tell = (StreamTellFn)string_stream_tell,
    .size = (StreamSizeFn)string_stream_size,
    .write = (StreamWriteFn)string_stream_write,
    .read = (StreamReadFn)string_stream_read,
    .delete_and_insert = (StreamDeleteAndInsertFn)string_stream_delete_and_insert,
};

Stream* string_stream_alloc() {
    StringStream* stream = malloc(sizeof(StringStream));
    stream->string = furry_string_alloc();
    stream->index = 0;
    stream->stream_base.vtable = &string_stream_vtable;
    return (Stream*)stream;
}

static void string_stream_free(StringStream* stream) {
    furry_string_free(stream->string);
    free(stream);
}

static bool string_stream_eof(StringStream* stream) {
    return (string_stream_tell(stream) >= string_stream_size(stream));
}

static void string_stream_clean(StringStream* stream) {
    stream->index = 0;
    furry_string_reset(stream->string);
}

static bool string_stream_seek(StringStream* stream, int32_t offset, StreamOffset offset_type) {
    bool result = true;
    switch(offset_type) {
    case StreamOffsetFromStart:
        if(offset >= 0) {
            stream->index = offset;
        } else {
            result = false;
            stream->index = 0;
        }
        break;
    case StreamOffsetFromCurrent:
        if(((int32_t)stream->index + offset) >= 0) {
            stream->index += offset;
        } else {
            result = false;
            stream->index = 0;
        }
        break;
    case StreamOffsetFromEnd:
        if(((int32_t)furry_string_size(stream->string) + offset) >= 0) {
            stream->index = furry_string_size(stream->string) + offset;
        } else {
            result = false;
            stream->index = 0;
        }
        break;
    }

    int32_t diff = (stream->index - furry_string_size(stream->string));
    if(diff > 0) {
        stream->index -= diff;
        result = false;
    }

    return result;
}

static size_t string_stream_tell(StringStream* stream) {
    return stream->index;
}

static size_t string_stream_size(StringStream* stream) {
    return furry_string_size(stream->string);
}

static size_t string_stream_write(StringStream* stream, const char* data, size_t size) {
    // TODO: can be optimized for edge cases
    size_t i;
    for(i = 0; i < size; i++) {
        string_stream_write_char(stream, data[i]);
    }

    return i;
}

static size_t string_stream_read(StringStream* stream, char* data, size_t size) {
    size_t write_index = 0;
    const char* cstr = furry_string_get_cstr(stream->string);

    if(!string_stream_eof(stream)) {
        while(true) {
            if(write_index >= size) break;

            data[write_index] = cstr[stream->index];
            write_index++;
            string_stream_seek(stream, 1, StreamOffsetFromCurrent);
            if(string_stream_eof(stream)) break;
        }
    }

    return write_index;
}

static bool string_stream_delete_and_insert(
    StringStream* stream,
    size_t delete_size,
    StreamWriteCB write_callback,
    const void* ctx) {
    bool result = true;

    if(delete_size) {
        size_t remain_size = string_stream_size(stream) - string_stream_tell(stream);
        remain_size = MIN(delete_size, remain_size);

        if(remain_size != 0) {
            furry_string_replace_at(stream->string, stream->index, remain_size, "");
        }
    }

    if(write_callback) {
        FurryString* right;
        right = furry_string_alloc_set(&furry_string_get_cstr(stream->string)[stream->index]);
        furry_string_left(stream->string, string_stream_tell(stream));
        result &= write_callback((Stream*)stream, ctx);
        furry_string_cat(stream->string, right);
        furry_string_free(right);
    }

    return result;
}

/**
 * Write to string stream helper
 * @param stream 
 * @param c 
 * @return size_t 
 */
static size_t string_stream_write_char(StringStream* stream, char c) {
    if(string_stream_eof(stream)) {
        furry_string_push_back(stream->string, c);
    } else {
        furry_string_set_char(stream->string, stream->index, c);
    }
    stream->index++;

    return 1;
}
