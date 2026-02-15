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

#include "./cobs.h"
#include "./span.h"
#include "./status.h"

#include <assert.h>
#include <stdint.h>

enum asrtl_chann_id_e
{
        ASRTL_META = 1,
        ASRTL_CORE = 2,
};

typedef uint16_t asrtl_chann_id;

typedef enum asrtl_status ( *asrtl_recv_callback )( void* ptr, struct asrtl_span buff );

/// Channel node representing one channel, chid should be unique within existing chain
struct asrtl_node
{
        asrtl_chann_id      chid;
        void*               recv_ptr;
        asrtl_recv_callback recv_cb;
        struct asrtl_node*  next;
};

/// Finds channel node with given id in linked list starting at head.
struct asrtl_node* asrtl_chann_find( struct asrtl_node* head, asrtl_chann_id id );

/// Dispatches received buffer to appropriate channel node in linked list starting at head.
enum asrtl_status asrtl_chann_dispatch( struct asrtl_node* head, struct asrtl_span buff );

enum asrtl_status asrtl_chann_cobs_dispatch(
    struct asrtl_cobs_ibuffer* ibuff,
    struct asrtl_node*         head,
    struct asrtl_span          in_buff );

typedef enum asrtl_status (
    *asrtl_send_callback )( void* ptr, asrtl_chann_id id, struct asrtl_span buff );

/// Sender structure holding callback and pointer for send operations.
struct asrtl_sender
{
        void*               ptr;
        asrtl_send_callback cb;
};

/// Sends buffer to channel with given id using sender's callback.
static inline enum asrtl_status asrtl_send(
    struct asrtl_sender* r,
    asrtl_chann_id       chid,
    struct asrtl_span    buff )
{
        assert( r );
        assert( r->cb );
        return r->cb( r->ptr, chid, buff );
}

#ifdef __cplusplus
}
#endif

#endif
