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

static enum asrtl_status asrtr_collect_client_send( void* p, struct asrtl_rec_span* buff )
{
        struct asrtr_collect_client* client = (struct asrtr_collect_client*) p;
        return asrtl_send( &client->sendr, ASRTL_COLL, buff, NULL, NULL );
}

// ---------------------------------------------------------------------------
// recv handlers (fast path)
// ---------------------------------------------------------------------------

static enum asrtl_status asrtr_collect_client_handle_ready(
    struct asrtr_collect_client* client,
    struct asrtl_span*           buff )
{
        if ( asrtl_span_unfit_for( buff, 8 ) ) {
                ASRTL_ERR_LOG( "asrtr_collect_client", "ready: message too short" );
                return ASRTL_RECV_ERR;
        }

        asrtl_cut_u32( &buff->b, &client->root_id );
        asrtl_cut_u32( &buff->b, &client->next_node_id );
        client->state = ASRTR_COLLECT_CLIENT_READY_RECV;
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtr_collect_client_handle_error(
    struct asrtr_collect_client* client,
    struct asrtl_span*           buff )
{
        (void) buff;
        ASRTL_ERR_LOG( "asrtr_collect_client", "error received from controller" );
        client->state = ASRTR_COLLECT_CLIENT_ERROR;
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtr_collect_client_recv( void* data, struct asrtl_span buff )
{
        struct asrtr_collect_client* client = (struct asrtr_collect_client*) data;

        if ( asrtl_span_unfit_for( &buff, 1 ) )
                return ASRTL_SUCCESS;
        asrtl_collect_message_id id = (asrtl_collect_message_id) *buff.b++;

        switch ( id ) {
        case ASRTL_COLLECT_MSG_READY:
                return asrtr_collect_client_handle_ready( client, &buff );
        case ASRTL_COLLECT_MSG_ERROR:
                return asrtr_collect_client_handle_error( client, &buff );
        default:
                ASRTL_ERR_LOG( "asrtr_collect_client", "unknown message id: %u", id );
                return ASRTL_RECV_UNEXPECTED_ERR;
        }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

static enum asrtl_status asrtr_collect_client_tick( struct asrtr_collect_client* client )
{
        if ( client->state != ASRTR_COLLECT_CLIENT_READY_RECV )
                return ASRTL_SUCCESS;

        enum asrtl_status st =
            asrtl_msg_rtoc_collect_ready_ack( asrtr_collect_client_send, client );
        if ( st != ASRTL_SUCCESS ) {
                ASRTL_ERR_LOG( "asrtr_collect_client", "tick: failed to send ready_ack" );
                return st;
        }

        client->state = ASRTR_COLLECT_CLIENT_ACTIVE;
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtr_collect_client_event( void* p, enum asrtl_event_e e, void* arg )
{
        struct asrtr_collect_client* client = (struct asrtr_collect_client*) p;
        switch ( e ) {
        case ASRTL_EVENT_TICK:
                return asrtr_collect_client_tick( client );
        case ASRTL_EVENT_RECV:
                return asrtr_collect_client_recv( client, *(struct asrtl_span*) arg );
        }
        ASRTL_ERR_LOG( "asrtr_collect_client", "unexpected event: %s", asrtl_event_to_str( e ) );
        return ASRTL_INVALID_EVENT_ERR;
}

enum asrtl_status asrtr_collect_client_init(
    struct asrtr_collect_client* client,
    struct asrtl_node*           prev,
    struct asrtl_sender          sender )
{
        if ( !client || !prev )
                return ASRTL_INIT_ERR;
        *client = ( struct asrtr_collect_client ){
            .node =
                ( struct asrtl_node ){
                    .chid     = ASRTL_COLL,
                    .e_cb_ptr = client,
                    .e_cb     = asrtr_collect_client_event,
                    .next     = NULL,
                },
            .sendr        = sender,
            .state        = ASRTR_COLLECT_CLIENT_IDLE,
            .root_id      = 0,
            .next_node_id = 1,
        };

        asrtl_node_link( prev, &client->node );
        return ASRTL_SUCCESS;
}

enum asrtl_status asrtr_collect_client_append(
    struct asrtr_collect_client*   client,
    asrtl_flat_id                  parent_id,
    char const*                    key,
    struct asrtl_flat_value const* value,
    asrtl_flat_id*                 out_id )
{
        if ( client->state == ASRTR_COLLECT_CLIENT_ERROR ) {
                ASRTL_ERR_LOG( "asrtr_collect_client", "append: in error state" );
                return ASRTL_ARG_ERR;
        }
        if ( client->state != ASRTR_COLLECT_CLIENT_ACTIVE ) {
                ASRTL_ERR_LOG( "asrtr_collect_client", "append: not active" );
                return ASRTL_ARG_ERR;
        }

        asrtl_flat_id     node_id = client->next_node_id++;
        enum asrtl_status st      = asrtl_msg_rtoc_collect_append(
            parent_id, node_id, key, value, asrtr_collect_client_send, client );
        if ( st == ASRTL_SUCCESS && out_id )
                *out_id = node_id;
        return st;
}

void asrtr_collect_client_deinit( struct asrtr_collect_client* client )
{
        if ( !client )
                return;
        asrtl_node_unlink( &client->node );
}
