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

#include "./status.h"
#include "util.h"

#include <assert.h>
#include <stdint.h>

enum asrtl_chann_id_e
{
        ASRTL_META = 1,
        ASRTL_CORE = 2,
};

typedef uint16_t asrtl_chann_id;

typedef enum asrtl_status ( *asrtl_rec_callback )( void* data, struct asrtl_span buff );

struct asrtl_node
{
        asrtl_chann_id     chid;
        void*              recv_data;
        asrtl_rec_callback recv_fn;
        struct asrtl_node* next;
};

typedef enum asrtl_status (
    *asrtl_send_callback )( void* data, asrtl_chann_id id, struct asrtl_span buff );

struct asrtl_sender
{
        void*               send_data;
        asrtl_send_callback send_fn;
};

static inline enum asrtl_status
asrtl_send( struct asrtl_sender* r, asrtl_chann_id chid, struct asrtl_span buff )
{
        assert( r );
        assert( r->send_fn );
        return r->send_fn( r->send_data, chid, buff );
}

#endif
