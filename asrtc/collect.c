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
#include "../asrtl/status_to_str.h"

#include <string.h>

// Minimum APPEND payload after msg_id: parent_id(4) + node_id(4) + key NUL(1) + type(1)
#define ASRTC_COLLECT_APPEND_MIN_LEN 10U

static char const* asrtc_collect_server_state_to_str( enum asrtc_collect_server_state state )
{
        switch ( state ) {
        case ASRTC_COLLECT_SERVER_IDLE:
                return "idle";
        case ASRTC_COLLECT_SERVER_READY_SENT:
                return "ready_sent";
        case ASRTC_COLLECT_SERVER_READY_ACK_RECV:
                return "ready_ack_recv";
        case ASRTC_COLLECT_SERVER_ACTIVE:
                return "active";
        }
        return "unknown";
}

static enum asrt_status asrtc_collect_server_send( void* p, struct asrt_rec_span* buff )
{
        struct asrtc_collect_server* server = (struct asrtc_collect_server*) p;
        return asrt_send( &server->sendr, ASRT_COLL, buff, NULL, NULL );
}

// ---------------------------------------------------------------------------
// recv handlers (fast path — store data, set pending)
// ---------------------------------------------------------------------------

static enum asrt_status asrtc_collect_server_handle_ready_ack(
    struct asrtc_collect_server* server,
    struct asrt_span*            buff )
{
        (void) buff;
        if ( server->state != ASRTC_COLLECT_SERVER_READY_SENT ) {
                ASRT_ERR_LOG(
                    "asrtc_collect_server",
                    "ready_ack: expected state ready_sent, got %s",
                    asrtc_collect_server_state_to_str( server->state ) );
                return ASRT_RECV_ERR;
        }
        server->state = ASRTC_COLLECT_SERVER_READY_ACK_RECV;
        return ASRT_SUCCESS;
}

static enum asrt_status asrtc_collect_server_handle_append(
    struct asrtc_collect_server* server,
    struct asrt_span*            buff )
{
        if ( server->state != ASRTC_COLLECT_SERVER_ACTIVE ) {
                ASRT_ERR_LOG(
                    "asrtc_collect_server",
                    "append: expected state active, got %s",
                    asrtc_collect_server_state_to_str( server->state ) );
                return ASRT_RECV_ERR;
        }

        uint32_t remaining = (uint32_t) ( buff->e - buff->b );
        if ( remaining < ASRTC_COLLECT_APPEND_MIN_LEN ) {
                ASRT_ERR_LOG( "asrtc_collect_server", "append: message too short" );
                return ASRT_RECV_ERR;
        }

        asrt_flat_id parent_id;
        asrt_flat_id node_id;
        asrt_cut_u32( &buff->b, &parent_id );
        asrt_cut_u32( &buff->b, &node_id );

        // Parse key\0
        uint8_t* nul = (uint8_t*) memchr( buff->b, '\0', (size_t) ( buff->e - buff->b ) );
        if ( !nul ) {
                ASRT_ERR_LOG( "asrtc_collect_server", "append: missing key terminator" );
                return ASRT_RECV_ERR;
        }
        char const* key = ( nul == buff->b ) ? NULL : (char const*) buff->b;
        buff->b         = nul + 1;

        // Parse type + value
        if ( buff->b >= buff->e ) {
                ASRT_ERR_LOG( "asrtc_collect_server", "append: missing type byte" );
                return ASRT_RECV_ERR;
        }

        uint8_t raw_type = *buff->b++;

        struct asrt_flat_value val;
        if ( raw_type == ASRT_FLAT_CTYPE_OBJECT || raw_type == ASRT_FLAT_CTYPE_ARRAY ) {
                val = ( struct asrt_flat_value ){ .type = (asrt_flat_value_type) raw_type };
        } else {
                enum asrt_status vst = asrt_flat_value_decode( buff, raw_type, &val );
                if ( vst != ASRT_SUCCESS ) {
                        ASRT_ERR_LOG( "asrtc_collect_server", "append: decode failure" );
                        return ASRT_RECV_ERR;
                }
        }

        // Append to tree
        enum asrt_status st;
        if ( raw_type == ASRT_FLAT_CTYPE_OBJECT || raw_type == ASRT_FLAT_CTYPE_ARRAY ) {
                st = asrt_flat_tree_append_cont(
                    &server->tree, parent_id, node_id, key, (asrt_flat_value_type) raw_type );
        } else {
                st = asrt_flat_tree_append_scalar(
                    &server->tree, parent_id, node_id, key, val.type, val.data.s );
        }
        if ( st != ASRT_SUCCESS ) {
                ASRT_ERR_LOG(
                    "asrtc_collect_server", "append: tree append failed for node %u", node_id );
                server->state = ASRTC_COLLECT_SERVER_IDLE;
                return asrt_msg_ctor_collect_error(
                    ASRT_COLLECT_ERR_NONE, asrtc_collect_server_send, server );
        }

        return ASRT_SUCCESS;
}

static enum asrt_status asrtc_collect_server_recv( void* data, struct asrt_span buff )
{
        struct asrtc_collect_server* server = (struct asrtc_collect_server*) data;

        if ( asrt_span_unfit_for( &buff, 1 ) )
                return ASRT_SUCCESS;
        asrt_collect_message_id id = (asrt_collect_message_id) *buff.b++;

        switch ( id ) {
        case ASRT_COLLECT_MSG_READY_ACK:
                return asrtc_collect_server_handle_ready_ack( server, &buff );
        case ASRT_COLLECT_MSG_APPEND:
                return asrtc_collect_server_handle_append( server, &buff );
        default:
                ASRT_ERR_LOG( "asrtc_collect_server", "unknown message id: %u", id );
                return ASRT_RECV_UNEXPECTED_ERR;
        }
}

static enum asrt_status asrtc_collect_server_tick_ready_ack( struct asrtc_collect_server* server )
{
        enum asrt_status dst = asrt_flat_tree_deinit( &server->tree );
        if ( dst != ASRT_SUCCESS ) {
                ASRT_ERR_LOG( "asrtc_collect_server", "ready_ack: tree deinit failed" );
                return dst;
        }

        enum asrt_status st = asrt_flat_tree_init(
            &server->tree, server->alloc, server->tree_block_cap, server->tree_node_cap );
        if ( st != ASRT_SUCCESS ) {
                ASRT_ERR_LOG( "asrtc_collect_server", "ready_ack: tree re-init failed" );
                return st;
        }

        server->state = ASRTC_COLLECT_SERVER_ACTIVE;
        if ( server->cmd.ack_cb ) {
                asrtc_collect_ready_ack_cb cb = server->cmd.ack_cb;
                void*                      p  = server->cmd.ack_cb_ptr;
                cb( p, ASRT_SUCCESS );
        }
        return ASRT_SUCCESS;
}

static enum asrt_status asrtc_collect_server_tick(
    struct asrtc_collect_server* server,
    uint32_t                     now )
{
        switch ( server->state ) {
        case ASRTC_COLLECT_SERVER_IDLE:
        case ASRTC_COLLECT_SERVER_ACTIVE:
                return ASRT_SUCCESS;

        case ASRTC_COLLECT_SERVER_READY_SENT:
                if ( server->cmd.deadline == 0 )
                        server->cmd.deadline = now + server->cmd.timeout;
                if ( now >= server->cmd.deadline ) {
                        asrtc_collect_ready_ack_cb cb = server->cmd.ack_cb;
                        void*                      p  = server->cmd.ack_cb_ptr;
                        server->state                 = ASRTC_COLLECT_SERVER_IDLE;
                        if ( cb )
                                cb( p, ASRT_TIMEOUT_ERR );
                }
                return ASRT_SUCCESS;

        case ASRTC_COLLECT_SERVER_READY_ACK_RECV:
                return asrtc_collect_server_tick_ready_ack( server );
        }
        return ASRT_SUCCESS;
}

static enum asrt_status asrtc_collect_server_event( void* p, enum asrt_event_e e, void* arg )
{
        struct asrtc_collect_server* server = (struct asrtc_collect_server*) p;
        switch ( e ) {
        case ASRT_EVENT_TICK:
                return asrtc_collect_server_tick( server, *(uint32_t*) arg );
        case ASRT_EVENT_RECV:
                return asrtc_collect_server_recv( server, *(struct asrt_span*) arg );
        }
        ASRT_ERR_LOG( "asrtc_collect", "unexpected event: %s", asrt_event_to_str( e ) );
        return ASRT_INVALID_EVENT_ERR;
}

enum asrt_status asrtc_collect_server_init(
    struct asrtc_collect_server* server,
    struct asrt_node*            prev,
    struct asrt_sender           sender,
    struct asrt_allocator        alloc,
    uint32_t                     tree_block_cap,
    uint32_t                     tree_node_cap )
{
        if ( !server || !prev )
                return ASRT_INIT_ERR;
        *server = ( struct asrtc_collect_server ){
            .node =
                ( struct asrt_node ){
                    .chid     = ASRT_COLL,
                    .e_cb_ptr = server,
                    .e_cb     = asrtc_collect_server_event,
                    .next     = NULL,
                },
            .sendr          = sender,
            .alloc          = alloc,
            .tree           = { 0 },
            .tree_block_cap = tree_block_cap,
            .tree_node_cap  = tree_node_cap,
            .state          = ASRTC_COLLECT_SERVER_IDLE,
            .next_node_id   = 1,
            .cmd            = { 0 },
        };

        enum asrt_status st =
            asrt_flat_tree_init( &server->tree, alloc, tree_block_cap, tree_node_cap );
        if ( st != ASRT_SUCCESS )
                return ASRT_INIT_ERR;

        asrt_node_link( prev, &server->node );
        return ASRT_SUCCESS;
}

enum asrt_status asrtc_collect_server_send_ready(
    struct asrtc_collect_server* server,
    asrt_flat_id                 root_id,
    uint32_t                     timeout,
    asrtc_collect_ready_ack_cb   ack_cb,
    void*                        ack_cb_ptr )
{
        if ( !server ) {
                ASRT_ERR_LOG( "asrtc_collect_server", "send_ready: server is NULL" );
                return ASRT_ARG_ERR;
        }
        if ( server->state != ASRTC_COLLECT_SERVER_IDLE &&
             server->state != ASRTC_COLLECT_SERVER_ACTIVE ) {
                ASRT_ERR_LOG(
                    "asrtc_collect_server",
                    "send_ready: expected state idle or active, got %s",
                    asrtc_collect_server_state_to_str( server->state ) );
                return ASRT_ARG_ERR;
        }

        server->state          = ASRTC_COLLECT_SERVER_READY_SENT;
        server->cmd.root_id    = root_id;
        server->cmd.ack_cb     = ack_cb;
        server->cmd.ack_cb_ptr = ack_cb_ptr;
        server->cmd.timeout    = timeout;
        server->cmd.deadline   = 0;
        return asrt_msg_ctor_collect_ready(
            root_id, server->next_node_id, asrtc_collect_server_send, server );
}

struct asrt_flat_tree const* asrtc_collect_server_tree( struct asrtc_collect_server const* server )
{
        return &server->tree;
}

void asrtc_collect_server_deinit( struct asrtc_collect_server* server )
{
        if ( !server )
                return;
        asrt_node_unlink( &server->node );
        asrt_flat_tree_deinit( &server->tree );
}
