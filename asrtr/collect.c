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

#include "../asrtl/asrt_assert.h"
#include "../asrtl/log.h"

// ---------------------------------------------------------------------------
// recv handlers (fast path)
// ---------------------------------------------------------------------------

static enum asrt_status asrt_collect_client_handle_ready(
    struct asrt_collect_client* client,
    struct asrt_span*           buff )
{
        if ( asrt_span_unfit_for( buff, 8 ) ) {
                ASRT_ERR_LOG( "asrt_collect_client", "ready: message too short" );
                return ASRT_RECV_ERR;
        }

        asrt_cut_u32( &buff->b, &client->root_id );
        asrt_cut_u32( &buff->b, &client->next_node_id );
        client->state = ASRT_COLLECT_CLIENT_READY_RECV;
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_collect_client_handle_error(
    struct asrt_collect_client* client,
    struct asrt_span*           buff )
{
        (void) buff;
        ASRT_ERR_LOG( "asrt_collect_client", "error received from controller" );
        client->state = ASRT_COLLECT_CLIENT_ERROR;
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_collect_client_recv( void* data, struct asrt_span buff )
{
        struct asrt_collect_client* client = (struct asrt_collect_client*) data;

        if ( asrt_span_unfit_for( &buff, 1 ) )
                return ASRT_SUCCESS;
        asrt_collect_message_id id = (asrt_collect_message_id) *buff.b++;

        switch ( id ) {
        case ASRT_COLLECT_MSG_READY:
                return asrt_collect_client_handle_ready( client, &buff );
        case ASRT_COLLECT_MSG_ERROR:
                return asrt_collect_client_handle_error( client, &buff );
        default:
                ASRT_ERR_LOG( "asrt_collect_client", "unknown message id: %u", id );
                return ASRT_RECV_UNEXPECTED_ERR;
        }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

static void asrt_collect_client_append_done( void* ptr, enum asrt_status st )
{
        struct asrt_collect_client* client = (struct asrt_collect_client*) ptr;
        ASRT_ASSERT( client->state == ASRT_COLLECT_CLIENT_APPEND_SENT );
        if ( st == ASRT_SUCCESS ) {
                client->state = ASRT_COLLECT_CLIENT_ACTIVE;
        } else {
                ASRT_ERR_LOG( "asrt_collect_client", "append send failed" );
                client->state = ASRT_COLLECT_CLIENT_ERROR;
        }
        if ( client->append_done_cb )
                client->append_done_cb( client->append_done_ptr, st );
}

static void asrt_collect_client_ready_ack_done( void* ptr, enum asrt_status st )
{
        struct asrt_collect_client* client = (struct asrt_collect_client*) ptr;
        ASRT_ASSERT( client->state == ASRT_COLLECT_CLIENT_READY_SENT );
        if ( st == ASRT_SUCCESS ) {
                client->state = ASRT_COLLECT_CLIENT_ACTIVE;
        } else {
                ASRT_ERR_LOG( "asrt_collect_client", "ready_ack send failed" );
                client->state = ASRT_COLLECT_CLIENT_IDLE;
        }
}

static enum asrt_status asrt_collect_client_tick( struct asrt_collect_client* client )
{
        if ( client->state != ASRT_COLLECT_CLIENT_READY_RECV )
                return ASRT_SUCCESS;

        client->state = ASRT_COLLECT_CLIENT_READY_SENT;
        asrt_send_enque(
            &client->node,
            asrt_msg_rtoc_collect_ready_ack( &client->ready_ack_msg ),
            asrt_collect_client_ready_ack_done,
            client );
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_collect_client_event( void* p, enum asrt_event_e e, void* arg )
{
        struct asrt_collect_client* client = (struct asrt_collect_client*) p;
        switch ( e ) {
        case ASRT_EVENT_TICK:
                return asrt_collect_client_tick( client );
        case ASRT_EVENT_RECV:
                return asrt_collect_client_recv( client, *(struct asrt_span*) arg );
        }
        ASRT_ERR_LOG( "asrt_collect_client", "unexpected event: %s", asrt_event_to_str( e ) );
        return ASRT_INVALID_EVENT_ERR;
}

enum asrt_status asrt_collect_client_init(
    struct asrt_collect_client* client,
    struct asrt_node*           prev )
{
        if ( !client || !prev )
                return ASRT_INIT_ERR;
        *client = ( struct asrt_collect_client ){
            .node =
                ( struct asrt_node ){
                    .chid       = ASRT_COLL,
                    .e_cb_ptr   = client,
                    .e_cb       = asrt_collect_client_event,
                    .next       = NULL,
                    .send_queue = prev->send_queue,
                },
            .state        = ASRT_COLLECT_CLIENT_IDLE,
            .root_id      = 0,
            .next_node_id = 1,
        };

        asrt_node_link( prev, &client->node );
        return ASRT_SUCCESS;
}

enum asrt_status asrt_collect_client_append(
    struct asrt_collect_client*   client,
    asrt_flat_id                  parent_id,
    char const*                   key,
    struct asrt_flat_value const* value,
    asrt_flat_id*                 out_id,
    asrt_send_done_cb             done_cb,
    void*                         done_ptr )
{
        if ( client->state != ASRT_COLLECT_CLIENT_ACTIVE ) {
                enum asrt_status ret =
                    client->state == ASRT_COLLECT_CLIENT_APPEND_SENT ? ASRT_BUSY_ERR : ASRT_ARG_ERR;
                ASRT_ERR_LOG(
                    "asrt_collect_client",
                    "append: called in unexpected state %d",
                    (int) client->state );
                return ret;
        }

        asrt_flat_id node_id    = client->next_node_id++;
        client->append_done_cb  = done_cb;
        client->append_done_ptr = done_ptr;
        client->state           = ASRT_COLLECT_CLIENT_APPEND_SENT;
        asrt_send_enque(
            &client->node,
            asrt_msg_rtoc_collect_append( &client->append_msg, parent_id, node_id, key, value ),
            asrt_collect_client_append_done,
            client );
        if ( out_id )
                *out_id = node_id;
        return ASRT_SUCCESS;
}

void asrt_collect_client_deinit( struct asrt_collect_client* client )
{
        if ( !client )
                return;
        asrt_node_unlink( &client->node );
}
