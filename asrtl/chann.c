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

#include "./log.h"
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

        if ( asrtl_span_unfit( &buff, sizeof( asrtl_chann_id ) ) )
                return ASRTL_SIZE_ERR;

        asrtl_chann_id id;
        asrtl_cut_u16( &buff.b, &id );

        ASRTL_DBG_LOG( "asrtl_chann", "Dispatch to channel %u of size %i", id, buff.e - buff.b );

        struct asrtl_node* p = asrtl_chann_find( head, id );
        if ( !p )
                return ASRTL_CHANN_NOT_FOUND;
        return p->recv_cb( p->recv_ptr, buff );
}

enum asrtl_status asrtl_chann_cobs_dispatch(
    struct asrtl_cobs_ibuffer* ibuff,
    struct asrtl_node*         head,
    struct asrtl_span          in_buff )
{
        asrtl_cobs_ibuffer_insert( ibuff, in_buff );

        for ( ;; ) {
                uint8_t           buffer[1024];
                struct asrtl_span sp = { .b = buffer, .e = buffer + sizeof buffer };
                int8_t            r  = asrtl_cobs_ibuffer_iter( ibuff, &sp );
                if ( r == -1 ) {
                        ASRTL_ERR_LOG( "test_rsim", "Received COBS message too large for buffer" );
                        return ASRTL_SIZE_ERR;
                }
                if ( r == 0 )
                        break;
                if ( sp.b == sp.e )
                        continue;
                asrtl_chann_dispatch( head, ( struct asrtl_span ){ .b = sp.b, .e = sp.e } );
        }

        return ASRTL_SUCCESS;
}
