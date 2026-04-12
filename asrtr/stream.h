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
#ifndef ASRTR_STREAM_H
#define ASRTR_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../asrtl/chann.h"
#include "../asrtl/stream_proto.h"
#include "./status.h"

#include <stdint.h>

typedef void ( *asrtr_stream_done_cb )( void* ptr, enum asrtl_status status );

/// Pending define operation.  The \c fields pointer is borrowed — the caller
/// must keep the pointed-to array alive until the done_cb fires (or tick()
/// returns when no callback is set).

struct asrtr_stream_pending_define
{
        uint8_t                             schema_id;
        uint8_t                             field_count;
        enum asrtl_strm_field_type_e const* fields;
};

/// Completed-send state.  Holds the transport result.
struct asrtr_stream_pending_done
{
        enum asrtl_status send_status;
};

enum asrtr_stream_client_state
{
        ASRTR_STRM_IDLE        = 0,
        ASRTR_STRM_DEFINE_SEND = 1,  ///< define() called, tick() will send.
        ASRTR_STRM_WAIT        = 2,  ///< send initiated, waiting for done_cb.
        ASRTR_STRM_DONE        = 3,  ///< send completed, tick() will fire user callback.
        ASRTR_STRM_ERROR       = 4,  ///< error received from controller.
};

/// Reactor-side stream client (ASRTL_STRM channel).
struct asrtr_stream_client
{
        struct asrtl_node   node;
        struct asrtl_sender sendr;

        enum asrtr_stream_client_state state;
        enum asrtl_strm_err_e          err_code;  ///< valid when state == ASRTR_STRM_ERROR.

        asrtr_stream_done_cb done_cb;      ///< user callback, fired in DONE state.
        void*                done_cb_ptr;  ///< user context for done_cb.

        union
        {
                struct asrtr_stream_pending_define define;
                struct asrtr_stream_pending_done   done;
        } op;
};

/// Initialise a stream client and link it into the node chain.
enum asrtr_status asrtr_stream_client_init(
    struct asrtr_stream_client* client,
    struct asrtl_node*          prev,
    struct asrtl_sender         sender );

/// Prepare a DEFINE message with a definition of schema. This will be sent from tick() and done
/// will be called once the sender acknowledges the send. The caller must keep the \c fields array
/// alive until the done_cb fires (or tick() returns when no callback is set).
enum asrtr_status asrtr_stream_client_define(
    struct asrtr_stream_client*         client,
    uint8_t                             schema_id,
    enum asrtl_strm_field_type_e const* fields,
    uint8_t                             field_count,
    asrtr_stream_done_cb                done_cb,
    void*                               done_cb_ptr );

/// Prepare a DATA message for a schema. The message will be sent from tick() and done will be
/// called once the sender acknowledges the send. The caller must keep the \c data array alive until
/// the done_cb fires (or tick() returns when no callback is set).
enum asrtr_status asrtr_stream_client_emit(
    struct asrtr_stream_client* client,
    uint8_t                     schema_id,
    uint8_t const*              data,
    uint16_t                    data_size,
    asrtr_stream_done_cb        done_cb,
    void*                       done_cb_ptr );

/// Reset the client to IDLE state.  Returns ASRTR_BUSY if a send
/// transaction is in progress (DEFINE_SEND or DEFINE_WAIT).
enum asrtr_status asrtr_stream_client_reset( struct asrtr_stream_client* client );

/// Process pending operations.  Drives the state machine one step.
enum asrtr_status asrtr_stream_client_tick( struct asrtr_stream_client* client );

#ifdef __cplusplus
}
#endif

#endif  // ASRTR_STREAM_H
