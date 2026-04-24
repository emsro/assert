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

#include "../asrtl/log.h"

#include <string.h>

static void asrtr_param_dispatch_cb(
    struct asrtr_param_client*     client,
    struct asrtr_param_query*      q,
    struct asrtl_flat_value const* val,
    int                            type_ok )
{
        static const struct asrtl_flat_value zero_val = { 0 };
        struct asrtl_flat_value const*       v        = type_ok ? val : &zero_val;

        switch ( q->expected_type ) {
        case ASRTL_FLAT_STYPE_NONE:
                if ( q->cb.any )
                        q->cb.any( client, q, *val );
                break;
        case ASRTL_FLAT_STYPE_U32:
                if ( q->cb.u32 )
                        q->cb.u32( client, q, v->data.s.u32_val );
                break;
        case ASRTL_FLAT_STYPE_I32:
                if ( q->cb.i32 )
                        q->cb.i32( client, q, v->data.s.i32_val );
                break;
        case ASRTL_FLAT_STYPE_STR:
                if ( q->cb.str )
                        q->cb.str( client, q, v->data.s.str_val );
                break;
        case ASRTL_FLAT_STYPE_FLOAT:
                if ( q->cb.flt )
                        q->cb.flt( client, q, v->data.s.float_val );
                break;
        case ASRTL_FLAT_STYPE_BOOL:
                if ( q->cb.bln )
                        q->cb.bln( client, q, v->data.s.bool_val );
                break;
        case ASRTL_FLAT_CTYPE_OBJECT:
                if ( q->cb.obj )
                        q->cb.obj( client, q, v->data.cont );
                break;
        case ASRTL_FLAT_CTYPE_ARRAY:
                if ( q->cb.arr )
                        q->cb.arr( client, q, v->data.cont );
                break;
        case ASRTL_FLAT_STYPE_NULL:
                ASRTL_ERR_LOG( "asrtr_param_client", "unsupported expected_type NULL" );
                break;
        default:
                break;
        }
}

static void asrtr_param_finish_query(
    struct asrtr_param_client*     client,
    struct asrtl_flat_value const* val )
{
        struct asrtr_param_query* q = client->pending_query;
        client->pending_query       = NULL;

        int has_error = q->error_code != 0;
        int type_ok   = !has_error && ( q->expected_type == ASRTL_FLAT_STYPE_NONE ||
                                      val->type == q->expected_type );
        if ( !has_error && !type_ok ) {
                ASRTL_ERR_LOG(
                    "asrtr_param_client",
                    "type mismatch: expected %u, got %u",
                    q->expected_type,
                    val->type );
                q->error_code = ASRTL_PARAM_ERR_TYPE_MISMATCH;
        }

        asrtr_param_dispatch_cb( client, q, val, type_ok );
}

static enum asrtl_status asrtr_param_client_send( void* p, struct asrtl_rec_span* buff )
{
        struct asrtr_param_client* client = (struct asrtr_param_client*) p;
        return asrtl_send( &client->sendr, ASRTL_PARA, buff, NULL, NULL );
}

// Cache lookup — walk cache_buf, parsing each node.
// On hit: deliver via finish_query.  On miss: clear cache + send wire QUERY.
//
// cache_buf layout: [node1][node2]...[nodeN][u32 wire_next_sibling_id]
// Each node: u32 node_id | key\0 | u8 type | type-specific value
// Nodes are present while (sp.e - sp.b) > 4.

enum asrtr_search_mode_e
{
        SEARCH_BY_NODE,
        SEARCH_BY_KEY,
};

static enum asrtl_status asrtr_cache_try_deliver(
    struct asrtr_param_client* client,
    enum asrtr_search_mode_e   mode )
{
        struct asrtl_span sp = {
            .b = client->cache_buf,
            .e = client->cache_buf + client->cache_len,
        };

        while ( (size_t) ( sp.e - sp.b ) > 4U ) {
                asrtl_flat_id nid;
                asrtl_cut_u32( &sp.b, &nid );

                size_t   search_len = (size_t) ( sp.e - sp.b ) - 4U;
                uint8_t* nul        = (uint8_t*) memchr( sp.b, '\0', search_len );
                if ( !nul ) {
                        ASRTL_ERR_LOG( "asrtr_param_client", "cache: missing key terminator" );
                        return ASRTL_RECV_ERR;
                }
                char const* key = (char const*) sp.b;
                sp.b            = nul + 1;

                if ( asrtl_span_unfit_for( &sp, 1 ) ) {
                        ASRTL_ERR_LOG( "asrtr_param_client", "cache: truncated node (no type)" );
                        return ASRTL_RECV_ERR;
                }
                uint8_t raw_type = *sp.b++;

                struct asrtl_flat_value val;
                enum asrtl_status       vst = asrtl_param_decode_value( &sp, raw_type, &val );
                if ( vst != ASRTL_SUCCESS ) {
                        ASRTL_ERR_LOG(
                            "asrtr_param_client", "cache: bad value (type %u)", raw_type );
                        return vst;
                }

                if ( ( mode == SEARCH_BY_NODE && nid == client->pending_query->node_id ) ||
                     ( mode == SEARCH_BY_KEY && strcmp( key, client->pending_query->key ) == 0 ) ) {
                        asrtl_flat_id next_sib;
                        if ( (size_t) ( sp.e - sp.b ) > 4U )
                                asrtl_u8d4_to_u32( sp.b, &next_sib );
                        else
                                next_sib = client->cache_next_sibling;
                        client->pending_query->node_id      = nid;
                        client->pending_query->key          = key;
                        client->pending_query->next_sibling = next_sib;

                        asrtr_param_finish_query( client, &val );
                        return ASRTL_SUCCESS;
                }
        }

        client->cache_len = 0;
        if ( mode == SEARCH_BY_KEY ) {
                struct asrtl_span buf = {
                    .b = client->cache_buf, .e = client->cache_buf + client->cache_capacity };
                return asrtl_msg_rtoc_param_find_by_key(
                    &buf,
                    client->pending_query->node_id,
                    client->pending_query->key,
                    asrtr_param_client_send,
                    client );
        }
        return asrtl_msg_rtoc_param_query(
            client->pending_query->node_id, asrtr_param_client_send, client );
}

static enum asrtl_status asrtr_param_client_handle_ready(
    struct asrtr_param_client* client,
    struct asrtl_span*         buff )
{
        if ( client->pending != ASRTR_PARAM_CLIENT_PENDING_NONE ) {
                ASRTL_ERR_LOG( "asrtr_param_client", "ready: pending event not consumed" );
                return ASRTL_RECV_ERR;
        }
        if ( asrtl_span_unfit_for( buff, 4 ) ) {
                ASRTL_ERR_LOG( "asrtr_param_client", "ready: message too short" );
                return ASRTL_RECV_ERR;
        }
        asrtl_flat_id root_id;
        asrtl_cut_u32( &buff->b, &root_id );

        client->pending              = ASRTR_PARAM_CLIENT_PENDING_READY;
        client->pending_data.root_id = root_id;
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtr_param_client_handle_response(
    struct asrtr_param_client* client,
    struct asrtl_span*         buff )
{
        if ( client->pending != ASRTR_PARAM_CLIENT_PENDING_NONE ) {
                ASRTL_ERR_LOG( "asrtr_param_client", "response: pending event not consumed" );
                return ASRTL_RECV_ERR;
        }
        uint32_t len = (uint32_t) ( buff->e - buff->b );
        if ( len > client->cache_capacity ) {
                ASRTL_ERR_LOG( "asrtr_param_client", "response: payload too large (%u)", len );
                return ASRTL_RECV_ERR;
        }
        if ( len < 4 ) {
                ASRTL_ERR_LOG( "asrtr_param_client", "response: too short" );
                return ASRTL_RECV_ERR;
        }
        memcpy( client->cache_buf, buff->b, len );
        client->cache_len = len;

        asrtl_u8d4_to_u32( client->cache_buf + len - 4, &client->cache_next_sibling );

        client->pending = ASRTR_PARAM_CLIENT_PENDING_DELIVER;
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtr_param_client_handle_error(
    struct asrtr_param_client* client,
    struct asrtl_span*         buff )
{
        if ( client->pending != ASRTR_PARAM_CLIENT_PENDING_NONE ) {
                ASRTL_ERR_LOG( "asrtr_param_client", "error: pending event not consumed" );
                return ASRTL_RECV_ERR;
        }
        if ( asrtl_span_unfit_for( buff, 5 ) ) {
                ASRTL_ERR_LOG( "asrtr_param_client", "error: message too short" );
                return ASRTL_RECV_ERR;
        }
        uint8_t       error_code = *buff->b++;
        asrtl_flat_id node_id;
        asrtl_cut_u32( &buff->b, &node_id );

        client->pending_data.error.error_code = error_code;
        client->pending_data.error.node_id    = node_id;
        client->pending                       = ASRTR_PARAM_CLIENT_PENDING_QUERY_ERROR;
        return ASRTL_SUCCESS;
}


static enum asrtl_status asrtr_param_client_tick( struct asrtr_param_client* client, uint32_t now )
{
        switch ( client->pending ) {
        case ASRTR_PARAM_CLIENT_PENDING_NONE:
                if ( client->pending_query && client->timeout > 0 ) {
                        struct asrtr_param_query* q = client->pending_query;
                        if ( q->start == 0 ) {
                                q->start = now;
                        } else if ( ( now - q->start ) >= client->timeout ) {
                                q->error_code                    = ASRTL_PARAM_ERR_TIMEOUT;
                                struct asrtl_flat_value zero_val = { 0 };
                                asrtr_param_finish_query( client, &zero_val );
                        }
                }
                return ASRTL_SUCCESS;

        case ASRTR_PARAM_CLIENT_PENDING_READY: {
                asrtl_flat_id root_id = client->pending_data.root_id;
                client->pending       = ASRTR_PARAM_CLIENT_PENDING_NONE;
                client->ready         = 0;
                client->root_id       = root_id;
                client->cache_len     = 0;

                enum asrtl_status st = asrtl_msg_rtoc_param_ready_ack(
                    client->cache_capacity, asrtr_param_client_send, client );
                if ( st != ASRTL_SUCCESS ) {
                        ASRTL_ERR_LOG( "asrtr_param_client", "ready: failed to send READY_ACK" );
                        return st;
                }
                client->ready = 1;
                return ASRTL_SUCCESS;
        }

        case ASRTR_PARAM_CLIENT_PENDING_DELIVER:
                client->pending = ASRTR_PARAM_CLIENT_PENDING_NONE;
                return asrtr_cache_try_deliver(
                    client, client->pending_query->key ? SEARCH_BY_KEY : SEARCH_BY_NODE );

        case ASRTR_PARAM_CLIENT_PENDING_QUERY_ERROR: {
                uint8_t       error_code          = client->pending_data.error.error_code;
                asrtl_flat_id node_id             = client->pending_data.error.node_id;
                client->pending                   = ASRTR_PARAM_CLIENT_PENDING_NONE;
                client->pending_query->error_code = error_code;
                client->pending_query->node_id    = node_id;

                struct asrtl_flat_value zero_val = { 0 };
                asrtr_param_finish_query( client, &zero_val );
                return ASRTL_SUCCESS;
        }
        }
        return ASRTL_SUCCESS;
}


static enum asrtl_status asrtr_param_client_recv( void* data, struct asrtl_span buff )
{
        struct asrtr_param_client* client = (struct asrtr_param_client*) data;
        asrtl_param_message_id     id;
        if ( asrtl_span_unfit_for( &buff, sizeof( id ) ) )
                return ASRTL_SUCCESS;
        id = (asrtl_param_message_id) *buff.b++;

        switch ( id ) {
        case ASRTL_PARAM_MSG_READY:
                return asrtr_param_client_handle_ready( client, &buff );
        case ASRTL_PARAM_MSG_RESPONSE:
                return asrtr_param_client_handle_response( client, &buff );
        case ASRTL_PARAM_MSG_ERROR:
                return asrtr_param_client_handle_error( client, &buff );
        default:
                ASRTL_ERR_LOG( "asrtr_param_client", "Unknown param message id: %u", id );
                return ASRTL_RECV_UNEXPECTED_ERR;
        }
}

static enum asrtl_status asrtr_param_client_event( void* p, enum asrtl_event_e e, void* arg )
{
        struct asrtr_param_client* client = (struct asrtr_param_client*) p;

        switch ( e ) {
        case ASRTL_EVENT_TICK:
                return asrtr_param_client_tick( client, *(uint32_t*) arg );
        case ASRTL_EVENT_RECV:
                return asrtr_param_client_recv( client, *(struct asrtl_span*) arg );
        default:
                break;
        }
        return ASRTL_SUCCESS;
}

enum asrtl_status asrtr_param_client_init(
    struct asrtr_param_client* client,
    struct asrtl_node*         prev,
    struct asrtl_sender        sender,
    struct asrtl_span          msg_buffer,
    uint32_t                   timeout )
{
        if ( !client || !prev || !msg_buffer.b || msg_buffer.e <= msg_buffer.b || timeout == 0 ) {
                ASRTL_ERR_LOG( "asrtr_param_client", "init: invalid arguments" );
                return ASRTL_INIT_ERR;
        }
        *client = ( struct asrtr_param_client ){
            .node =
                ( struct asrtl_node ){
                    .chid     = ASRTL_PARA,
                    .e_cb_ptr = client,
                    .e_cb     = asrtr_param_client_event,
                    .next     = NULL,
                },
            .sendr              = sender,
            .root_id            = ASRTL_PARAM_NONE_ID,
            .ready              = 0,
            .cache_buf          = msg_buffer.b,
            .cache_capacity     = (uint32_t) ( msg_buffer.e - msg_buffer.b ),
            .cache_len          = 0,
            .cache_next_sibling = ASRTL_PARAM_NONE_ID,
            .pending_query      = NULL,
            .timeout            = timeout,
            .pending            = ASRTR_PARAM_CLIENT_PENDING_NONE,
        };
        asrtl_node_link( prev, &client->node );
        return ASRTL_SUCCESS;
}

asrtl_flat_id asrtr_param_client_root_id( struct asrtr_param_client const* client )
{
        return client->root_id;
}

enum asrtl_status asrtr_param_client_query(
    struct asrtr_param_query*  query,
    struct asrtr_param_client* client,
    asrtl_flat_id              node_id,
    char const*                key )
{
        if ( !client->ready ) {
                ASRTL_ERR_LOG( "asrtr_param_client", "query: not ready" );
                return ASRTL_ARG_ERR;
        }
        if ( !query ) {
                ASRTL_ERR_LOG( "asrtr_param_client", "query: null query" );
                return ASRTL_ARG_ERR;
        }
        if ( client->pending_query ) {
                ASRTL_ERR_LOG( "asrtr_param_client", "query: another query is pending" );
                return ASRTL_ARG_ERR;
        }

        client->pending_query = query;
        query->error_code     = 0;
        query->node_id        = node_id;
        query->key            = key;
        query->next_sibling   = ASRTL_PARAM_NONE_ID;
        query->start          = 0;
        client->pending       = ASRTR_PARAM_CLIENT_PENDING_DELIVER;
        return ASRTL_SUCCESS;
}

void asrtr_param_client_deinit( struct asrtr_param_client* client )
{
        if ( !client )
                return;
        asrtl_node_unlink( &client->node );
}
