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
#include "chann.h"

#include "./util.h"

#include <stddef.h>

struct asrtl_node* asrtl_chann_find( struct asrtl_node* head, asrtl_chann_id id )
{
        for ( struct asrtl_node* p = head; p; p = p->next )
                if ( p->chid == id )
                        return p;
        return NULL;
}

enum asrtl_status asrtl_chann_dispatch( struct asrtl_node* head, struct asrtl_span buff )
{
        if ( !head )
                return ASRTL_RECV_INTERNAL_ERR;

        if ( asrtl_buffer_unfit( &buff, sizeof( asrtl_chann_id ) ) )
                return ASRTL_SIZE_ERR;

        asrtl_chann_id id;
        asrtl_cut_u16( &buff.b, &id );

        struct asrtl_node* p = asrtl_chann_find( head, id );
        if ( !p )
                return ASRTL_CHANN_NOT_FOUND;
        return p->recv_cb( p->recv_ptr, buff );
}
