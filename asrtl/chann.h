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

enum asrt_chann_id_e
{
        ASRT_META = 1,
        ASRT_CORE = 2,
        ASRT_DIAG = 3,
        ASRT_PARA = 4,
        ASRT_COLL = 5,
        ASRT_STRM = 6,
};

typedef uint16_t asrt_chann_id;

enum asrt_event_e
{
        ASRT_EVENT_TICK = 1,
        ASRT_EVENT_RECV = 2,
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

struct asrt_send_req
{
        struct asrt_span_span buff;
        asrt_chann_id         chid;
        asrt_send_done_cb     done_cb;
        void*                 done_ptr;

        struct asrt_send_req* next;
};

struct asrt_send_req_list
{
        struct asrt_send_req* head;
        struct asrt_send_req* tail;
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

/// Channel node representing one channel, chid should be unique within existing chain
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
static inline void asrt_node_link( struct asrt_node* after, struct asrt_node* node )
{
        ASRT_ASSERT( after );
        ASRT_ASSERT( node );
        node->next = after->next;
        node->prev = after;
        if ( after->next != NULL )
                after->next->prev = node;
        after->next = node;
}

/// Remove `node` from the chain, patching prev/next neighbours.
static inline void asrt_node_unlink( struct asrt_node* node )
{
        ASRT_ASSERT( node );
        if ( node->prev != NULL )
                node->prev->next = node->next;
        if ( node->next != NULL )
                node->next->prev = node->prev;
        node->next = NULL;
        node->prev = NULL;
}

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

void asrt_chann_tick_successors( struct asrt_node* node, uint32_t now );

/// Finds channel node with given id in linked list starting at head.
struct asrt_node* asrt_chann_find( struct asrt_node* head, asrt_chann_id id );

/// Dispatches received buffer to appropriate channel node in linked list starting at head.
enum asrt_status asrt_chann_dispatch( struct asrt_node* head, struct asrt_span buff );

enum asrt_status asrt_chann_cobs_dispatch(
    struct asrt_cobs_ibuffer* ibuff,
    struct asrt_node*         head,
    struct asrt_span          in_buff );

/// Enqueue a send request on the node's send queue, to be sent out in order.  The caller should
/// setup the buffer in the req struct before calling this. Channel ID and done callback are set by
/// this function.
static inline void asrt_send_enque(
    struct asrt_node*     node,
    struct asrt_send_req* req,
    asrt_send_done_cb     done_cb,
    void*                 done_ptr )
{
        struct asrt_send_req_list* list = node->send_queue;
        if ( list->tail != NULL )
                list->tail->next = req;
        else
                list->head = req;
        list->tail    = req;
        req->next     = NULL;
        req->chid     = node->chid;
        req->done_cb  = done_cb;
        req->done_ptr = done_ptr;
}

/// Return the next pending send request without removing it, or NULL if the list is empty.
/// The caller inspects req->buff / req->chid and performs the actual send.
static inline struct asrt_send_req* asrt_send_req_list_next( struct asrt_send_req_list* list )
{
        return list->head;
}

/// Mark the head request as completed: remove it from the list, free the slot (req->next = NULL),
/// and invoke its done_cb (if set) with the supplied status.
/// Behaviour is undefined if the list is empty.
static inline void asrt_send_req_list_done(
    struct asrt_send_req_list* list,
    enum asrt_status           status )
{
        struct asrt_send_req* req = list->head;
        ASRT_ASSERT( req != NULL );
        list->head = req->next;
        if ( list->head == NULL )
                list->tail = NULL;
        req->next = NULL;  // free the slot
        if ( req->done_cb )
                req->done_cb( req->done_ptr, status );
}

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
