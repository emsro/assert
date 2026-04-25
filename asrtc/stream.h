/// Permission to use, copy, modify, and/or distribute this software for any
/// purpose with or without fee is hereby granted.
///
/// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
/// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
/// AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
/// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
/// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
/// OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
/// PERFORMANCE OF THIS SOFTWARE.
#ifndef ASRTC_STREAM_H
#define ASRTC_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../asrtl/allocator.h"
#include "../asrtl/chann.h"
#include "../asrtl/stream_proto.h"

#include <stdint.h>

#define ASRTC_STREAM_MAX_SCHEMAS 255

/// Descriptor for a defined schema.
struct asrtc_stream_schema
{
        uint8_t                      schema_id;    ///< Schema ID as defined by the reactor.
        uint8_t                      field_count;  ///< Number of fields.
        uint16_t                     record_size;  ///< Sum of field wire sizes.
        enum asrt_strm_field_type_e* fields;       ///< Allocated array of field type tags.
        struct asrtc_stream_record*  first;        ///< Head of record linked list.
        struct asrtc_stream_record*  last;         ///< Tail of record linked list.
        uint32_t                     count;        ///< Number of records in the list.
};

/// A single stored record with a separately allocated data buffer.
struct asrtc_stream_record
{
        struct asrtc_stream_record* next;
        uint8_t*                    data;  ///< record_size bytes of raw field data.
};

/// Collection of stream schemas — used both internally by the server and as
/// the return value of take().
struct asrtc_stream_schemas
{
        struct asrtc_stream_schema* schemas;       ///< Allocated array of schemas.
        uint32_t                    schema_count;  ///< Number of schemas in the array.
        struct asrt_allocator       alloc;         ///< Allocator used for all contained memory.
};

/// Controller-side stream server (ASRT_STRM channel).
///
/// Receives DEFINE messages to register schemas and DATA messages carrying raw
/// records.  Records are stored in per-schema linked lists and handed out to
/// the user via take semantics.
///
/// Typical lifecycle:
///   1. Reactor sends DEFINE(schema_id, fields[...]).
///   2. Reactor sends DATA(schema_id, raw bytes) — one record per message.
///   3. After test ends, controller takes all schemas via take().
///   4. Free the result with asrtc_stream_schemas_free().
struct asrtc_stream_server
{
        struct asrt_node      node;
        struct asrt_sender    sendr;
        struct asrt_allocator alloc;

        struct asrtc_stream_schema* lookup[ASRTC_STREAM_MAX_SCHEMAS];  ///< Internal lookup by ID.
};

/// Initialise a stream server and link it into the node chain.
enum asrt_status asrtc_stream_server_init(
    struct asrtc_stream_server* server,
    struct asrt_node*           prev,
    struct asrt_sender          sender,
    struct asrt_allocator       alloc );

/// Take all defined schemas and their records.
///
/// Returns a struct containing an allocated array of all schemas.  The server
/// is cleared after the call.  The caller owns the result and must free it
/// with asrtc_stream_schemas_free().
/// Returns schema_count=0 and schemas=NULL if no schemas exist.
struct asrtc_stream_schemas asrtc_stream_server_take( struct asrtc_stream_server* server );

/// Free a schemas collection — all schemas, their records, and the array itself.
void asrtc_stream_schemas_free( struct asrtc_stream_schemas* schemas );

/// Clear all schemas and records (e.g. on test boundary).
void asrtc_stream_server_clear( struct asrtc_stream_server* server );

/// Free all resources.
void asrtc_stream_server_deinit( struct asrtc_stream_server* server );

#ifdef __cplusplus
}
#endif

#endif  // ASRTC_STREAM_H
