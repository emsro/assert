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

struct asrt_node* asrt_chann_find( struct asrt_node* head, asrt_chann_id id )
{
        for ( struct asrt_node* p = head; p; p = p->next )
                if ( p->chid == id )
                        return p;
        return NULL;
}

enum asrt_status asrt_chann_dispatch( struct asrt_node* head, struct asrt_span buff )
{
        if ( !head )
                return ASRT_RECV_INTERNAL_ERR;

        if ( asrt_span_unfit_for( &buff, sizeof( asrt_chann_id ) ) )
                return ASRT_SIZE_ERR;

        asrt_chann_id id;
        asrt_cut_u16( &buff.b, &id );

        ASRT_DBG_LOG( "asrt_chann", "Dispatch to channel %u of size %i", id, buff.e - buff.b );

        struct asrt_node* p = asrt_chann_find( head, id );
        if ( !p )
                return ASRT_CHANN_NOT_FOUND;
        return asrt_chann_recv( p, buff );
}

enum asrt_status asrt_chann_cobs_dispatch(
    struct asrt_cobs_ibuffer* ibuff,
    struct asrt_node*         head,
    struct asrt_span          in_buff )
{
        enum asrt_status st = asrt_cobs_ibuffer_insert( ibuff, in_buff );
        if ( st != ASRT_SUCCESS )
                return st;

        for ( ;; ) {
                uint8_t          buffer[1024];
                struct asrt_span sp = { .b = buffer, .e = buffer + sizeof buffer };
                int8_t           r  = asrt_cobs_ibuffer_iter( ibuff, &sp );
                if ( r == -1 ) {
                        ASRT_ERR_LOG( "asrt_chann", "Received COBS message too large for buffer" );
                        return ASRT_SIZE_ERR;
                }
                if ( r == 0 )
                        break;
                if ( sp.b == sp.e )
                        continue;
                st = asrt_chann_dispatch( head, ( struct asrt_span ){ .b = sp.b, .e = sp.e } );
                if ( st != ASRT_SUCCESS )
                        return st;
        }

        return ASRT_SUCCESS;
}

void asrt_chann_tick_successors( struct asrt_node* node, uint32_t now )
{
        for ( struct asrt_node* p = node; p != NULL; p = p->next ) {
                enum asrt_status const s = asrt_chann_tick( p, now );
                if ( s != ASRT_SUCCESS ) {
                        ASRT_ERR_LOG(
                            "asrt_chann",
                            "Tick failed for channel %u: %s",
                            p->chid,
                            asrt_status_to_str( s ) );
                }
        }
}
