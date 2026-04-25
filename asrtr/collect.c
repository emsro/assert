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
#include "./collect.h"

#include "../asrtl/log.h"

static enum asrt_status asrtr_collect_client_send( void* p, struct asrt_rec_span* buff )
{
        struct asrtr_collect_client* client = (struct asrtr_collect_client*) p;
        return asrt_send( &client->sendr, ASRT_COLL, buff, NULL, NULL );
}

// ---------------------------------------------------------------------------
// recv handlers (fast path)
// ---------------------------------------------------------------------------

static enum asrt_status asrtr_collect_client_handle_ready(
    struct asrtr_collect_client* client,
    struct asrt_span*            buff )
{
        if ( asrt_span_unfit_for( buff, 8 ) ) {
                ASRT_ERR_LOG( "asrtr_collect_client", "ready: message too short" );
                return ASRT_RECV_ERR;
        }

        asrt_cut_u32( &buff->b, &client->root_id );
        asrt_cut_u32( &buff->b, &client->next_node_id );
        client->state = ASRTR_COLLECT_CLIENT_READY_RECV;
        return ASRT_SUCCESS;
}

static enum asrt_status asrtr_collect_client_handle_error(
    struct asrtr_collect_client* client,
    struct asrt_span*            buff )
{
        (void) buff;
        ASRT_ERR_LOG( "asrtr_collect_client", "error received from controller" );
        client->state = ASRTR_COLLECT_CLIENT_ERROR;
        return ASRT_SUCCESS;
}

static enum asrt_status asrtr_collect_client_recv( void* data, struct asrt_span buff )
{
        struct asrtr_collect_client* client = (struct asrtr_collect_client*) data;

        if ( asrt_span_unfit_for( &buff, 1 ) )
                return ASRT_SUCCESS;
        asrt_collect_message_id id = (asrt_collect_message_id) *buff.b++;

        switch ( id ) {
        case ASRT_COLLECT_MSG_READY:
                return asrtr_collect_client_handle_ready( client, &buff );
        case ASRT_COLLECT_MSG_ERROR:
                return asrtr_collect_client_handle_error( client, &buff );
        default:
                ASRT_ERR_LOG( "asrtr_collect_client", "unknown message id: %u", id );
                return ASRT_RECV_UNEXPECTED_ERR;
        }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

static enum asrt_status asrtr_collect_client_tick( struct asrtr_collect_client* client )
{
        if ( client->state != ASRTR_COLLECT_CLIENT_READY_RECV )
                return ASRT_SUCCESS;

        enum asrt_status st = asrt_msg_rtoc_collect_ready_ack( asrtr_collect_client_send, client );
        if ( st != ASRT_SUCCESS ) {
                ASRT_ERR_LOG( "asrtr_collect_client", "tick: failed to send ready_ack" );
                return st;
        }

        client->state = ASRTR_COLLECT_CLIENT_ACTIVE;
        return ASRT_SUCCESS;
}

static enum asrt_status asrtr_collect_client_event( void* p, enum asrt_event_e e, void* arg )
{
        struct asrtr_collect_client* client = (struct asrtr_collect_client*) p;
        switch ( e ) {
        case ASRT_EVENT_TICK:
                return asrtr_collect_client_tick( client );
        case ASRT_EVENT_RECV:
                return asrtr_collect_client_recv( client, *(struct asrt_span*) arg );
        }
        ASRT_ERR_LOG( "asrtr_collect_client", "unexpected event: %s", asrt_event_to_str( e ) );
        return ASRT_INVALID_EVENT_ERR;
}

enum asrt_status asrtr_collect_client_init(
    struct asrtr_collect_client* client,
    struct asrt_node*            prev,
    struct asrt_sender           sender )
{
        if ( !client || !prev )
                return ASRT_INIT_ERR;
        *client = ( struct asrtr_collect_client ){
            .node =
                ( struct asrt_node ){
                    .chid     = ASRT_COLL,
                    .e_cb_ptr = client,
                    .e_cb     = asrtr_collect_client_event,
                    .next     = NULL,
                },
            .sendr        = sender,
            .state        = ASRTR_COLLECT_CLIENT_IDLE,
            .root_id      = 0,
            .next_node_id = 1,
        };

        asrt_node_link( prev, &client->node );
        return ASRT_SUCCESS;
}

enum asrt_status asrtr_collect_client_append(
    struct asrtr_collect_client*  client,
    asrt_flat_id                  parent_id,
    char const*                   key,
    struct asrt_flat_value const* value,
    asrt_flat_id*                 out_id )
{
        if ( client->state == ASRTR_COLLECT_CLIENT_ERROR ) {
                ASRT_ERR_LOG( "asrtr_collect_client", "append: in error state" );
                return ASRT_ARG_ERR;
        }
        if ( client->state != ASRTR_COLLECT_CLIENT_ACTIVE ) {
                ASRT_ERR_LOG( "asrtr_collect_client", "append: not active" );
                return ASRT_ARG_ERR;
        }

        asrt_flat_id     node_id = client->next_node_id++;
        enum asrt_status st      = asrt_msg_rtoc_collect_append(
            parent_id, node_id, key, value, asrtr_collect_client_send, client );
        if ( st == ASRT_SUCCESS && out_id )
                *out_id = node_id;
        return st;
}

void asrtr_collect_client_deinit( struct asrtr_collect_client* client )
{
        if ( !client )
                return;
        asrt_node_unlink( &client->node );
}
