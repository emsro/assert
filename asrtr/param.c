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
#include "./param.h"

#include "../asrtl/asrt_assert.h"
#include "../asrtl/log.h"

#include <string.h>

static void asrt_param_ready_ack_done( void* ptr, enum asrt_status st )
{
        struct asrt_param_client* client = (struct asrt_param_client*) ptr;
        ASRT_ASSERT( client->state == ASRT_PARAM_CLIENT_READY_SENT );
        if ( st == ASRT_SUCCESS )
                client->ready = 1;
        else
                ASRT_ERR_LOG( "asrt_param_client", "ready_ack send failed" );
        client->state = ASRT_PARAM_CLIENT_IDLE;
}

static void asrt_param_dispatch_cb(
    struct asrt_param_client*     client,
    struct asrt_param_query*      q,
    struct asrt_flat_value const* val,
    int                           type_ok )
{
        static const struct asrt_flat_value zero_val = { 0 };
        struct asrt_flat_value const*       v        = type_ok ? val : &zero_val;

        switch ( q->expected_type ) {
        case ASRT_FLAT_STYPE_NONE:
                if ( q->cb.any )
                        q->cb.any( client, q, *val );
                break;
        case ASRT_FLAT_STYPE_U32:
                if ( q->cb.u32 )
                        q->cb.u32( client, q, v->data.s.u32_val );
                break;
        case ASRT_FLAT_STYPE_I32:
                if ( q->cb.i32 )
                        q->cb.i32( client, q, v->data.s.i32_val );
                break;
        case ASRT_FLAT_STYPE_STR:
                if ( q->cb.str )
                        q->cb.str( client, q, v->data.s.str_val );
                break;
        case ASRT_FLAT_STYPE_FLOAT:
                if ( q->cb.flt )
                        q->cb.flt( client, q, v->data.s.float_val );
                break;
        case ASRT_FLAT_STYPE_BOOL:
                if ( q->cb.bln )
                        q->cb.bln( client, q, v->data.s.bool_val );
                break;
        case ASRT_FLAT_CTYPE_OBJECT:
                if ( q->cb.obj )
                        q->cb.obj( client, q, v->data.cont );
                break;
        case ASRT_FLAT_CTYPE_ARRAY:
                if ( q->cb.arr )
                        q->cb.arr( client, q, v->data.cont );
                break;
        case ASRT_FLAT_STYPE_NULL:
                ASRT_ERR_LOG( "asrt_param_client", "unsupported expected_type NULL" );
                break;
        default:
                break;
        }
}

static void asrt_param_finish_query(
    struct asrt_param_client*     client,
    struct asrt_flat_value const* val )
{
        struct asrt_param_query* q = client->pending_query;
        client->pending_query      = NULL;

        int has_error = q->error_code != 0;
        int type_ok   = !has_error &&
                      ( q->expected_type == ASRT_FLAT_STYPE_NONE || val->type == q->expected_type );
        if ( !has_error && !type_ok ) {
                ASRT_ERR_LOG(
                    "asrt_param_client",
                    "type mismatch: expected %u, got %u",
                    q->expected_type,
                    val->type );
                q->error_code = ASRT_PARAM_ERR_TYPE_MISMATCH;
        }

        asrt_param_dispatch_cb( client, q, val, type_ok );
}

// Cache lookup — walk cache_buf, parsing each node.
// On hit: deliver via finish_query.  On miss: clear cache + send wire QUERY.
//
// cache_buf layout: [node1][node2]...[nodeN][u32 wire_next_sibling_id]
// Each node: u32 node_id | key\0 | u8 type | type-specific value
// Nodes are present while (sp.e - sp.b) > 4.

enum asrt_search_mode_e
{
        SEARCH_BY_NODE,
        SEARCH_BY_KEY,
};

static enum asrt_status asrt_cache_try_deliver(
    struct asrt_param_client* client,
    enum asrt_search_mode_e   mode )
{
        struct asrt_span sp = {
            .b = client->cache_buf,
            .e = client->cache_buf + client->cache_len,
        };

        while ( (size_t) ( sp.e - sp.b ) > 4U ) {
                asrt_flat_id nid;
                asrt_cut_u32( &sp.b, &nid );

                size_t   search_len = (size_t) ( sp.e - sp.b ) - 4U;
                uint8_t* nul        = (uint8_t*) memchr( sp.b, '\0', search_len );
                if ( !nul ) {
                        ASRT_ERR_LOG( "asrt_param_client", "cache: missing key terminator" );
                        return ASRT_RECV_ERR;
                }
                char const* key = (char const*) sp.b;
                sp.b            = nul + 1;

                if ( asrt_span_unfit_for( &sp, 1 ) ) {
                        ASRT_ERR_LOG( "asrt_param_client", "cache: truncated node (no type)" );
                        return ASRT_RECV_ERR;
                }
                uint8_t raw_type = *sp.b++;

                struct asrt_flat_value val;
                enum asrt_status       vst = asrt_param_decode_value( &sp, raw_type, &val );
                if ( vst != ASRT_SUCCESS ) {
                        ASRT_ERR_LOG( "asrt_param_client", "cache: bad value (type %u)", raw_type );
                        return vst;
                }

                if ( ( mode == SEARCH_BY_NODE && nid == client->pending_query->node_id ) ||
                     ( mode == SEARCH_BY_KEY && strcmp( key, client->pending_query->key ) == 0 ) ) {
                        asrt_flat_id next_sib;
                        if ( (size_t) ( sp.e - sp.b ) > 4U )
                                asrt_u8d4_to_u32( sp.b, &next_sib );
                        else
                                next_sib = client->cache_next_sibling;
                        client->pending_query->node_id      = nid;
                        client->pending_query->key          = key;
                        client->pending_query->next_sibling = next_sib;

                        asrt_param_finish_query( client, &val );
                        return ASRT_SUCCESS;
                }
        }

        client->cache_len = 0;
        if ( mode == SEARCH_BY_KEY ) {
                asrt_send_enque(
                    &client->node,
                    asrt_msg_rtoc_param_find_by_key(
                        &client->find_by_key_msg,
                        client->pending_query->node_id,
                        client->pending_query->key ),
                    NULL,
                    NULL );
                return ASRT_SUCCESS;
        }
        asrt_send_enque(
            &client->node,
            asrt_msg_rtoc_param_query( &client->query_msg, client->pending_query->node_id ),
            NULL,
            NULL );
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_param_client_handle_ready(
    struct asrt_param_client* client,
    struct asrt_span*         buff )
{
        if ( client->state != ASRT_PARAM_CLIENT_IDLE ) {
                ASRT_ERR_LOG( "asrt_param_client", "ready: pending event not consumed" );
                return ASRT_RECV_ERR;
        }
        if ( asrt_span_unfit_for( buff, 4 ) ) {
                ASRT_ERR_LOG( "asrt_param_client", "ready: message too short" );
                return ASRT_RECV_ERR;
        }
        asrt_flat_id root_id;
        asrt_cut_u32( &buff->b, &root_id );

        client->state              = ASRT_PARAM_CLIENT_READY_RECV;
        client->state_data.root_id = root_id;
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_param_client_handle_response(
    struct asrt_param_client* client,
    struct asrt_span*         buff )
{
        if ( client->state != ASRT_PARAM_CLIENT_IDLE ) {
                ASRT_ERR_LOG( "asrt_param_client", "response: pending event not consumed" );
                return ASRT_RECV_ERR;
        }
        uint32_t len = (uint32_t) ( buff->e - buff->b );
        if ( len > client->cache_capacity ) {
                ASRT_ERR_LOG( "asrt_param_client", "response: payload too large (%u)", len );
                return ASRT_RECV_ERR;
        }
        if ( len < 4 ) {
                ASRT_ERR_LOG( "asrt_param_client", "response: too short" );
                return ASRT_RECV_ERR;
        }
        memcpy( client->cache_buf, buff->b, len );
        client->cache_len = len;

        asrt_u8d4_to_u32( client->cache_buf + len - 4, &client->cache_next_sibling );

        client->state = ASRT_PARAM_CLIENT_DELIVER;
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_param_client_handle_error(
    struct asrt_param_client* client,
    struct asrt_span*         buff )
{
        if ( client->state != ASRT_PARAM_CLIENT_IDLE ) {
                ASRT_ERR_LOG( "asrt_param_client", "error: pending event not consumed" );
                return ASRT_RECV_ERR;
        }
        if ( asrt_span_unfit_for( buff, 5 ) ) {
                ASRT_ERR_LOG( "asrt_param_client", "error: message too short" );
                return ASRT_RECV_ERR;
        }
        uint8_t      error_code = *buff->b++;
        asrt_flat_id node_id;
        asrt_cut_u32( &buff->b, &node_id );

        client->state_data.error.error_code = error_code;
        client->state_data.error.node_id    = node_id;
        client->state                       = ASRT_PARAM_CLIENT_QUERY_ERROR;
        return ASRT_SUCCESS;
}


static enum asrt_status asrt_param_client_tick( struct asrt_param_client* client, uint32_t now )
{
        switch ( client->state ) {
        case ASRT_PARAM_CLIENT_IDLE:
                if ( client->pending_query && client->timeout > 0 ) {
                        struct asrt_param_query* q = client->pending_query;
                        if ( q->start == 0 ) {
                                q->start = now;
                        } else if ( ( now - q->start ) >= client->timeout ) {
                                q->error_code                   = ASRT_PARAM_ERR_TIMEOUT;
                                struct asrt_flat_value zero_val = { 0 };
                                asrt_param_finish_query( client, &zero_val );
                        }
                }
                return ASRT_SUCCESS;

        case ASRT_PARAM_CLIENT_READY_RECV: {
                asrt_flat_id root_id = client->state_data.root_id;
                client->state        = ASRT_PARAM_CLIENT_READY_SENT;
                client->ready        = 0;
                client->root_id      = root_id;
                client->cache_len    = 0;

                asrt_send_enque(
                    &client->node,
                    asrt_msg_rtoc_param_ready_ack( &client->ready_ack_msg, client->cache_capacity ),
                    asrt_param_ready_ack_done,
                    client );
                return ASRT_SUCCESS;
        }

        case ASRT_PARAM_CLIENT_READY_SENT:
                return ASRT_SUCCESS;

        case ASRT_PARAM_CLIENT_DELIVER:
                client->state = ASRT_PARAM_CLIENT_IDLE;
                return asrt_cache_try_deliver(
                    client, client->pending_query->key ? SEARCH_BY_KEY : SEARCH_BY_NODE );

        case ASRT_PARAM_CLIENT_QUERY_ERROR: {
                uint8_t      error_code           = client->state_data.error.error_code;
                asrt_flat_id node_id              = client->state_data.error.node_id;
                client->state                     = ASRT_PARAM_CLIENT_IDLE;
                client->pending_query->error_code = error_code;
                client->pending_query->node_id    = node_id;

                struct asrt_flat_value zero_val = { 0 };
                asrt_param_finish_query( client, &zero_val );
                return ASRT_SUCCESS;
        }
        }
        return ASRT_SUCCESS;
}


static enum asrt_status asrt_param_client_recv( void* data, struct asrt_span buff )
{
        struct asrt_param_client* client = (struct asrt_param_client*) data;
        asrt_param_message_id     id;
        if ( asrt_span_unfit_for( &buff, sizeof( id ) ) )
                return ASRT_SUCCESS;
        id = (asrt_param_message_id) *buff.b++;

        switch ( id ) {
        case ASRT_PARAM_MSG_READY:
                return asrt_param_client_handle_ready( client, &buff );
        case ASRT_PARAM_MSG_RESPONSE:
                return asrt_param_client_handle_response( client, &buff );
        case ASRT_PARAM_MSG_ERROR:
                return asrt_param_client_handle_error( client, &buff );
        default:
                ASRT_ERR_LOG( "asrt_param_client", "Unknown param message id: %u", id );
                return ASRT_RECV_UNEXPECTED_ERR;
        }
}

static enum asrt_status asrt_param_client_event( void* p, enum asrt_event_e e, void* arg )
{
        struct asrt_param_client* client = (struct asrt_param_client*) p;

        switch ( e ) {
        case ASRT_EVENT_TICK:
                return asrt_param_client_tick( client, *(uint32_t*) arg );
        case ASRT_EVENT_RECV:
                return asrt_param_client_recv( client, *(struct asrt_span*) arg );
        default:
                break;
        }
        return ASRT_SUCCESS;
}

enum asrt_status asrt_param_client_init(
    struct asrt_param_client* client,
    struct asrt_node*         prev,
    struct asrt_span          msg_buffer,
    uint32_t                  timeout )
{
        if ( !client || !prev || !msg_buffer.b || msg_buffer.e <= msg_buffer.b || timeout == 0 ) {
                ASRT_ERR_LOG( "asrt_param_client", "init: invalid arguments" );
                return ASRT_INIT_ERR;
        }
        *client = ( struct asrt_param_client ){
            .node =
                ( struct asrt_node ){
                    .chid       = ASRT_PARA,
                    .e_cb_ptr   = client,
                    .e_cb       = asrt_param_client_event,
                    .next       = NULL,
                    .send_queue = prev->send_queue,
                },
            .root_id            = ASRT_PARAM_NONE_ID,
            .ready              = 0,
            .cache_buf          = msg_buffer.b,
            .cache_capacity     = (uint32_t) ( msg_buffer.e - msg_buffer.b ),
            .cache_len          = 0,
            .cache_next_sibling = ASRT_PARAM_NONE_ID,
            .pending_query      = NULL,
            .timeout            = timeout,
            .state              = ASRT_PARAM_CLIENT_IDLE,
        };
        asrt_node_link( prev, &client->node );
        return ASRT_SUCCESS;
}

asrt_flat_id asrt_param_client_root_id( struct asrt_param_client const* client )
{
        return client->root_id;
}

enum asrt_status asrt_param_client_query(
    struct asrt_param_query*  query,
    struct asrt_param_client* client,
    asrt_flat_id              node_id,
    char const*               key )
{
        if ( !client->ready ) {
                ASRT_ERR_LOG( "asrt_param_client", "query: not ready" );
                return ASRT_ARG_ERR;
        }
        if ( !query ) {
                ASRT_ERR_LOG( "asrt_param_client", "query: null query" );
                return ASRT_ARG_ERR;
        }
        if ( client->pending_query ) {
                ASRT_ERR_LOG( "asrt_param_client", "query: another query is pending" );
                return ASRT_ARG_ERR;
        }

        client->pending_query = query;
        query->error_code     = 0;
        query->node_id        = node_id;
        query->key            = key;
        query->next_sibling   = ASRT_PARAM_NONE_ID;
        query->start          = 0;
        client->state         = ASRT_PARAM_CLIENT_DELIVER;
        return ASRT_SUCCESS;
}

void asrt_param_client_deinit( struct asrt_param_client* client )
{
        if ( !client )
                return;
        asrt_node_unlink( &client->node );
}
