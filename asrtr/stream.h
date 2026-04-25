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
#ifndef ASRT_STREAM_H
#define ASRT_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../asrtl/chann.h"
#include "../asrtl/stream_proto.h"

#include <stdint.h>

typedef void ( *asrt_stream_done_cb )( void* ptr, enum asrt_status status );

/// Pending define operation.  The \c fields pointer is borrowed — the caller
/// must keep the pointed-to array alive until the done_cb fires (or tick()
/// returns when no callback is set).

struct asrt_stream_pending_define
{
        uint8_t                            schema_id;
        uint8_t                            field_count;
        enum asrt_strm_field_type_e const* fields;
};

/// Completed-send state.  Holds the transport result.
struct asrt_stream_pending_done
{
        enum asrt_status send_status;
};

enum asrt_stream_client_state
{
        ASRT_STRM_IDLE        = 0,
        ASRT_STRM_DEFINE_SEND = 1,  ///< define() called, tick() will send.
        ASRT_STRM_WAIT        = 2,  ///< send initiated, waiting for done_cb.
        ASRT_STRM_DONE        = 3,  ///< send completed, tick() will fire user callback.
        ASRT_STRM_ERROR       = 4,  ///< error received from controller.
};

/// Reactor-side stream client (ASRT_STRM channel).
struct asrt_stream_client
{
        struct asrt_node   node;
        struct asrt_sender sendr;

        enum asrt_stream_client_state state;
        enum asrt_strm_err_e          err_code;  ///< valid when state == ASRT_STRM_ERROR.

        asrt_stream_done_cb done_cb;      ///< user callback, fired in DONE state.
        void*               done_cb_ptr;  ///< user context for done_cb.

        union
        {
                struct asrt_stream_pending_define define;
                struct asrt_stream_pending_done   done;
        } op;
};

/// Initialise a stream client and link it into the node chain.
enum asrt_status asrt_stream_client_init(
    struct asrt_stream_client* client,
    struct asrt_node*          prev,
    struct asrt_sender         sender );

/// Deinitialise a stream client and unlink it from the node chain.
void asrt_stream_client_deinit( struct asrt_stream_client* client );

/// Prepare a DEFINE message with a definition of schema. This will be sent from tick() and done
/// will be called once the sender acknowledges the send. The caller must keep the \c fields array
/// alive until the done_cb fires (or tick() returns when no callback is set).
enum asrt_status asrt_stream_client_define(
    struct asrt_stream_client*         client,
    uint8_t                            schema_id,
    enum asrt_strm_field_type_e const* fields,
    uint8_t                            field_count,
    asrt_stream_done_cb                done_cb,
    void*                              done_cb_ptr );

/// Prepare a DATA message for a schema. The message will be sent from tick() and done will be
/// called once the sender acknowledges the send. The caller must keep the \c data array alive until
/// the done_cb fires (or tick() returns when no callback is set).
enum asrt_status asrt_stream_client_emit(
    struct asrt_stream_client* client,
    uint8_t                    schema_id,
    uint8_t const*             data,
    uint16_t                   data_size,
    asrt_stream_done_cb        done_cb,
    void*                      done_cb_ptr );

/// Reset the client to IDLE state.  Returns ASRT_BUSY if a send
/// transaction is in progress (DEFINE_SEND or DEFINE_WAIT).
enum asrt_status asrt_stream_client_reset( struct asrt_stream_client* client );

#ifdef __cplusplus
}
#endif

#endif  // ASRT_STREAM_H
