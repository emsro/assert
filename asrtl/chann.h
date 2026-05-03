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
#ifndef ASRT_CHANN_H
#define ASRT_CHANN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "./asrt_assert.h"
#include "./cobs.h"
#include "./log.h"
#include "./span.h"
#include "./status.h"
#include "./status_to_str.h"

#include <stddef.h>
#include <stdint.h>

/// Well-known channel IDs.
enum asrt_chann_id_e
{
        ASRT_META = 1,  ///< Reserved / meta channel (not used by user code).
        ASRT_CORE = 2,  ///< Core channel: test enumeration and execution.
        ASRT_DIAG = 3,  ///< Diagnostic channel: assertion records from the target.
        ASRT_PARA = 4,  ///< Parameter channel: structured key/value queries.
        ASRT_COLL = 5,  ///< Collector channel: test output tree from the target.
        ASRT_STRM = 6,  ///< Stream channel: high-throughput typed records from the target.
};

typedef uint16_t asrt_chann_id;

/// Events dispatched to channel nodes via their event callback.
enum asrt_event_e
{
        ASRT_EVENT_TICK = 1,  ///< Periodic tick; arg is a uint32_t* carrying the current time.
        ASRT_EVENT_RECV = 2,  ///< Incoming message; arg is an asrt_span* with the payload.
};

static inline char const* asrt_event_to_str( enum asrt_event_e event )
{
        switch ( event ) {
        case ASRT_EVENT_TICK:
                return "tick";
        case ASRT_EVENT_RECV:
                return "recv";
        }
        return "unknown";
}

/// Callback invoked when a send operation completes.
typedef void ( *asrt_send_done_cb )( void* ptr, enum asrt_status status );

/// An outgoing message request placed in a module's send queue.
struct asrt_send_req
{
        struct asrt_span_span buff;      ///< Message payload (scatter-gather).
        asrt_chann_id         chid;      ///< Target channel ID, set by asrt_send_enque().
        asrt_send_done_cb     done_cb;   ///< Completion callback, may be NULL.
        void*                 done_ptr;  ///< Opaque context forwarded to done_cb.

        struct asrt_send_req* next;  ///< Intrusive linked-list link.
};

/// Intrusive FIFO of outgoing send requests.  Shared between a module and the
/// transport layer: the module enqueues, the transport dequeues and sends.
struct asrt_send_req_list
{
        struct asrt_send_req* head;  ///< Oldest pending request (next to transmit).
        struct asrt_send_req* tail;  ///< Newest pending request.
};

static inline void asrt_send_req_list_init( struct asrt_send_req_list* list )
{
        list->head = NULL;
        list->tail = NULL;
}

static inline int32_t asrt_send_is_req_used(
    struct asrt_send_req_list const* list,
    struct asrt_send_req const*      req )
{
        return req->next != NULL || list->tail == req;
}

typedef enum asrt_status ( *asrt_event_callback )( void* ptr, enum asrt_event_e event, void* arg );

/// A node in a doubly-linked channel chain.
///
/// Each protocol module (reactor, diag client, param client, …) embeds one
/// asrt_node as its first member.  Modules are connected into a chain with
/// asrt_node_link(); the chain is the runtime representation of the protocol
/// stack for one side of the connection.
///
/// Events flow through the chain via two mechanisms:
///
///   RECV — asrt_chann_cobs_dispatch() decodes incoming bytes from the
///          transport, extracts a channel ID from each frame header, and
///          calls asrt_chann_dispatch() which walks the chain to find the
///          node whose chid matches and delivers the payload to its e_cb.
///          Each node therefore only sees frames addressed to its own channel.
///
///   TICK — asrt_chann_tick_successors() calls e_cb(ASRT_EVENT_TICK) on
///          every node after the head.  Modules use the tick to advance
///          state machines, check timeouts, and drain their send queue.
///
/// The send_queue pointer is shared across all nodes in the chain.  Any
/// module that wants to transmit calls asrt_send_enque(), which appends to
/// the shared queue; the transport layer drains it independently.
///
/// chid must be unique within a chain — asrt_chann_find() and the dispatch
/// path rely on this to route frames correctly.
struct asrt_node
{
        asrt_chann_id              chid;
        void*                      e_cb_ptr;
        asrt_event_callback        e_cb;
        struct asrt_node*          next;
        struct asrt_node*          prev;
        struct asrt_send_req_list* send_queue;
};

/// Insert `node` after `after` in the chain.
void asrt_node_link( struct asrt_node* after, struct asrt_node* node );

/// Remove `node` from the chain, patching prev/next neighbours.
void asrt_node_unlink( struct asrt_node* node );

static inline enum asrt_status asrt_chann_recv( struct asrt_node* node, struct asrt_span buff )
{
        ASRT_ASSERT( node );
        ASRT_ASSERT( node->e_cb );
        return node->e_cb( node->e_cb_ptr, ASRT_EVENT_RECV, &buff );
}

static inline enum asrt_status asrt_chann_tick( struct asrt_node* node, uint32_t now )
{
        ASRT_ASSERT( node );
        ASRT_ASSERT( node->e_cb );
        return node->e_cb( node->e_cb_ptr, ASRT_EVENT_TICK, &now );
}

/// Tick all successor nodes in the chain (starting from node->next).
/// Propagates the current time to every node after @p node.
void asrt_chann_tick_successors( struct asrt_node* node, uint32_t now );

/// Finds channel node with given id in linked list starting at head.
struct asrt_node* asrt_chann_find( struct asrt_node* head, asrt_chann_id id );

/// Dispatches received buffer to appropriate channel node in linked list starting at head.
enum asrt_status asrt_chann_dispatch( struct asrt_node* head, struct asrt_span buff );

/// Feed raw bytes from @p in_buff into the COBS input buffer, decode complete
/// frames, and dispatch each decoded frame to the appropriate channel node.
/// Handles the zero-terminator byte that follows each COBS frame automatically.
enum asrt_status asrt_chann_cobs_dispatch(
    struct asrt_cobs_ibuffer* ibuff,
    struct asrt_node*         head,
    struct asrt_span          in_buff );

/// Enqueue a send request on the node's send queue, to be sent out in order.  The caller should
/// setup the buffer in the req struct before calling this. Channel ID and done callback are set by
/// this function.
void asrt_send_enque(
    struct asrt_node*     node,
    struct asrt_send_req* req,
    asrt_send_done_cb     done_cb,
    void*                 done_ptr );

/// Return the next pending send request without removing it, or NULL if the list is empty.
/// The caller inspects req->buff / req->chid and performs the actual send.
static inline struct asrt_send_req* asrt_send_req_list_next( struct asrt_send_req_list* list )
{
        return list->head;
}

/// Mark the head request as completed: remove it from the list, free the slot (req->next = NULL),
/// and invoke its done_cb (if set) with the supplied status.
/// Behaviour is undefined if the list is empty.
void asrt_send_req_list_done( struct asrt_send_req_list* list, enum asrt_status status );

struct asrt_u8d1msg
{
        uint8_t              buff[1];
        struct asrt_send_req req;
};
struct asrt_u8d2msg
{
        uint8_t              buff[2];
        struct asrt_send_req req;
};
struct asrt_u8d4msg
{
        uint8_t              buff[4];
        struct asrt_send_req req;
};
struct asrt_u8d5msg
{
        uint8_t              buff[5];
        struct asrt_send_req req;
};
struct asrt_u8d6msg
{
        uint8_t              buff[6];
        struct asrt_send_req req;
};
struct asrt_u8d8msg
{
        uint8_t              buff[8];
        struct asrt_send_req req;
};
struct asrt_u8d9msg
{
        uint8_t              buff[9];
        struct asrt_send_req req;
};

#ifdef __cplusplus
}
#endif

#endif
