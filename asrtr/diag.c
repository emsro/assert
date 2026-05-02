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
#include "./diag.h"

#include "../asrtl/chann.h"
#include "../asrtl/diag_proto.h"
#include "../asrtl/log.h"

static enum asrt_status asrt_diag_event( void* p, enum asrt_event_e e, void* arg )
{
        (void) arg;  // diag event handler does not use the callback argument
        (void) p;    // or the node pointer
        switch ( e ) {
        case ASRT_EVENT_TICK:
                return ASRT_SUCCESS;
        case ASRT_EVENT_RECV:
                ASRT_ERR_LOG( "asrt_diag", "Received unexpected message on diag channel" );
                return ASRT_ARG_ERR;
        }
        return ASRT_SUCCESS;
}

enum asrt_status asrt_diag_client_init( struct asrt_diag_client* diag, struct asrt_node* prev )
{
        if ( !diag || !prev ) {
                ASRT_ERR_LOG( "asrt_diag", "Invalid arguments to diag init" );
                return ASRT_INIT_ERR;
        }
        *diag = ( struct asrt_diag_client ){
            .node =
                ( struct asrt_node ){
                    .chid       = ASRT_DIAG,
                    .e_cb_ptr   = diag,
                    .e_cb       = asrt_diag_event,
                    .next       = NULL,
                    .send_queue = prev->send_queue,
                },
        };
        asrt_node_link( prev, &diag->node );
        return ASRT_SUCCESS;
}

void asrt_diag_client_deinit( struct asrt_diag_client* diag )
{
        if ( !diag )
                return;
        asrt_node_unlink( &diag->node );
}

enum asrt_diag_record_result asrt_diag_client_record(
    struct asrt_diag_client* diag,
    char const*              file,
    uint32_t                 line,
    char const*              extra,
    asrt_diag_record_done_cb done_cb,
    void*                    done_ptr )
{
        ASRT_ASSERT( diag );
        ASRT_ASSERT( file );

        ASRT_INF_LOG( "asrt_diag", "Sending diag message: %s:%u", file, line );

        if ( asrt_send_is_req_used( diag->node.send_queue, &diag->msg.req ) ) {
                ASRT_ERR_LOG( "asrt_diag", "diag slot busy, dropping record" );
                return ASRT_DIAG_RECORD_BUSY;
        }
        asrt_send_enque(
            &diag->node,
            asrt_msg_rtoc_diag_record( &diag->msg, file, line, extra ),
            done_cb,
            done_ptr );
        return ASRT_DIAG_RECORD_ACCEPTED;
}
