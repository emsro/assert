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
#ifndef ASRTL_CHANN_H
#define ASRTL_CHANN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "./asrtl_assert.h"
#include "./cobs.h"
#include "./log.h"
#include "./span.h"
#include "./status.h"
#include "./status_to_str.h"

#include <stddef.h>
#include <stdint.h>

enum asrtl_chann_id_e
{
        ASRTL_META = 1,
        ASRTL_CORE = 2,
        ASRTL_DIAG = 3,
        ASRTL_PARA = 4,
        ASRTL_COLL = 5,
        ASRTL_STRM = 6,
};

typedef uint16_t asrtl_chann_id;

enum asrtl_event_e
{
        ASRTL_EVENT_TICK = 1,
        ASRTL_EVENT_RECV = 2,
};

static inline char const* asrtl_event_to_str( enum asrtl_event_e event )
{
        switch ( event ) {
        case ASRTL_EVENT_TICK:
                return "tick";
        case ASRTL_EVENT_RECV:
                return "recv";
        }
        return "unknown";
}

typedef enum asrtl_status (
    *asrtl_event_callback )( void* ptr, enum asrtl_event_e event, void* arg );

/// Channel node representing one channel, chid should be unique within existing chain
struct asrtl_node
{
        asrtl_chann_id       chid;
        void*                e_cb_ptr;
        asrtl_event_callback e_cb;
        struct asrtl_node*   next;
        struct asrtl_node*   prev;
};

/// Insert `node` after `after` in the chain.
static inline void asrtl_node_link( struct asrtl_node* after, struct asrtl_node* node )
{
        ASRTL_ASSERT( after );
        ASRTL_ASSERT( node );
        node->next = after->next;
        node->prev = after;
        if ( after->next != NULL )
                after->next->prev = node;
        after->next = node;
}

/// Remove `node` from the chain, patching prev/next neighbours.
static inline void asrtl_node_unlink( struct asrtl_node* node )
{
        ASRTL_ASSERT( node );
        if ( node->prev != NULL )
                node->prev->next = node->next;
        if ( node->next != NULL )
                node->next->prev = node->prev;
        node->next = NULL;
        node->prev = NULL;
}

static inline enum asrtl_status asrtl_chann_recv( struct asrtl_node* node, struct asrtl_span buff )
{
        ASRTL_ASSERT( node );
        ASRTL_ASSERT( node->e_cb );
        return node->e_cb( node->e_cb_ptr, ASRTL_EVENT_RECV, &buff );
}

static inline enum asrtl_status asrtl_chann_tick( struct asrtl_node* node, uint32_t now )
{
        ASRTL_ASSERT( node );
        ASRTL_ASSERT( node->e_cb );
        return node->e_cb( node->e_cb_ptr, ASRTL_EVENT_TICK, &now );
}

void asrtl_chann_tick_successors( struct asrtl_node* node, uint32_t now );

/// Finds channel node with given id in linked list starting at head.
struct asrtl_node* asrtl_chann_find( struct asrtl_node* head, asrtl_chann_id id );

/// Dispatches received buffer to appropriate channel node in linked list starting at head.
enum asrtl_status asrtl_chann_dispatch( struct asrtl_node* head, struct asrtl_span buff );

enum asrtl_status asrtl_chann_cobs_dispatch(
    struct asrtl_cobs_ibuffer* ibuff,
    struct asrtl_node*         head,
    struct asrtl_span          in_buff );

/// Callback invoked when a send operation completes.
typedef void ( *asrtl_send_done_cb )( void* ptr, enum asrtl_status status );

typedef enum asrtl_status ( *asrtl_send_callback )(
    void*                  ptr,
    asrtl_chann_id         id,
    struct asrtl_rec_span* buff,
    asrtl_send_done_cb     done_cb,
    void*                  done_ptr );

/// Sender structure holding callback and pointer for send operations.
struct asrtl_sender
{
        void*               ptr;
        asrtl_send_callback cb;
};

/// Sends buffer to channel with given id using sender's callback.
static inline enum asrtl_status asrtl_send(
    struct asrtl_sender*   r,
    asrtl_chann_id         chid,
    struct asrtl_rec_span* buff,
    asrtl_send_done_cb     done_cb,
    void*                  done_ptr )
{
        ASRTL_ASSERT( r );
        ASRTL_ASSERT( r->cb );
        return r->cb( r->ptr, chid, buff, done_cb, done_ptr );
}

#ifdef __cplusplus
}
#endif

#endif
