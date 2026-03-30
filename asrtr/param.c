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

static void asrtr_param_finish_query(
    struct asrtr_param_client*     client,
    struct asrtl_flat_value const* val )
{
        struct asrtr_param_query* q = client->pending_query;
        client->pending_query       = NULL;

        int has_error = q->error_code != 0;
        int type_ok   = !has_error && ( q->expected_type == ASRTL_FLAT_VALUE_TYPE_NONE ||
                                      val->type == q->expected_type );
        if ( !has_error && !type_ok ) {
                ASRTL_ERR_LOG(
                    "asrtr_param_client",
                    "type mismatch: expected %u, got %u",
                    q->expected_type,
                    val->type );
                q->error_code = ASRTR_PARAM_ERR_TYPE_MISMATCH;
        }

        switch ( q->expected_type ) {
        case ASRTL_FLAT_VALUE_TYPE_NONE:
                if ( q->cb.any )
                        q->cb.any( client, q, *val );
                break;
        case ASRTL_FLAT_VALUE_TYPE_U32:
                if ( q->cb.u32 )
                        q->cb.u32( client, q, type_ok ? val->u32_val : 0 );
                break;
        case ASRTL_FLAT_VALUE_TYPE_I32:
                if ( q->cb.i32 )
                        q->cb.i32( client, q, type_ok ? val->i32_val : 0 );
                break;
        case ASRTL_FLAT_VALUE_TYPE_STR:
                if ( q->cb.str )
                        q->cb.str( client, q, type_ok ? val->str_val : NULL );
                break;
        case ASRTL_FLAT_VALUE_TYPE_FLOAT:
                if ( q->cb.flt )
                        q->cb.flt( client, q, type_ok ? val->float_val : 0.0f );
                break;
        case ASRTL_FLAT_VALUE_TYPE_BOOL:
                if ( q->cb.bln )
                        q->cb.bln( client, q, type_ok ? val->bool_val : 0 );
                break;
        case ASRTL_FLAT_VALUE_TYPE_OBJECT: {
                if ( q->cb.obj ) {
                        struct asrtl_flat_child_list zero = { 0, 0 };
                        q->cb.obj( client, q, type_ok ? val->obj_val : zero );
                }
                break;
        }
        case ASRTL_FLAT_VALUE_TYPE_ARRAY: {
                if ( q->cb.arr ) {
                        struct asrtl_flat_child_list zero = { 0, 0 };
                        q->cb.arr( client, q, type_ok ? val->arr_val : zero );
                }
                break;
        }
        case ASRTL_FLAT_VALUE_TYPE_NULL:
                ASRTL_ERR_LOG( "asrtr_param_client", "unsupported expected_type NULL" );
                break;
        }
}

static enum asrtl_status asrtr_param_client_send( void* p, struct asrtl_rec_span* buff )
{
        struct asrtr_param_client* client = (struct asrtr_param_client*) p;
        return asrtl_send( &client->sendr, ASRTL_PARAM, buff );
}

// ---------------------------------------------------------------------------
// Cache lookup — walk cache_buf, parsing each node.
// On hit: deliver via finish_query.  On miss: clear cache + send wire QUERY.
//
// cache_buf layout: [node1][node2]...[nodeN][u32 wire_next_sibling_id]
// Each node: u32 node_id | key\0 | u8 type | type-specific value
// Nodes are present while (sp.e - sp.b) > 4.
// ---------------------------------------------------------------------------

static enum asrtl_status asrtr_cache_try_deliver( struct asrtr_param_client* client )
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

                if ( nid == client->pending_query->node_id ) {
                        asrtl_flat_id next_sib;
                        if ( (size_t) ( sp.e - sp.b ) > 4U )
                                asrtl_u8d4_to_u32( sp.b, &next_sib );
                        else
                                next_sib = client->cache_next_sibling;
                        client->pending_query->key          = key;
                        client->pending_query->next_sibling = next_sib;

                        asrtr_param_finish_query( client, &val );
                        return ASRTL_SUCCESS;
                }
        }

        client->cache_len = 0;
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
        client->pending                       = ASRTR_PARAM_CLIENT_PENDING_ERROR;
        return ASRTL_SUCCESS;
}

// ---------------------------------------------------------------------------
// tick — heavy processing
// ---------------------------------------------------------------------------

enum asrtl_status asrtr_param_client_tick( struct asrtr_param_client* client )
{
        switch ( client->pending ) {
        case ASRTR_PARAM_CLIENT_PENDING_NONE:
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
                return asrtr_cache_try_deliver( client );

        case ASRTR_PARAM_CLIENT_PENDING_ERROR: {
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

// ---------------------------------------------------------------------------
// recv dispatch
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// public API
// ---------------------------------------------------------------------------

enum asrtr_status asrtr_param_client_init(
    struct asrtr_param_client* client,
    struct asrtl_node*         prev,
    struct asrtl_sender        sender,
    struct asrtl_span          msg_buffer )
{
        if ( !client || !prev || !msg_buffer.b || msg_buffer.e <= msg_buffer.b ) {
                ASRTL_ERR_LOG( "asrtr_param_client", "init: invalid arguments" );
                return ASRTR_INIT_ERR;
        }
        *client = ( struct asrtr_param_client ){
            .node =
                ( struct asrtl_node ){
                    .chid     = ASRTL_PARAM,
                    .recv_ptr = client,
                    .recv_cb  = asrtr_param_client_recv,
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
            .pending            = ASRTR_PARAM_CLIENT_PENDING_NONE,
        };
        prev->next = &client->node;
        return ASRTR_SUCCESS;
}

asrtl_flat_id asrtr_param_client_root_id( struct asrtr_param_client const* client )
{
        return client->root_id;
}

enum asrtl_status asrtr_param_client_query(
    struct asrtr_param_query*  query,
    struct asrtr_param_client* client,
    asrtl_flat_id              node_id )
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
        query->key            = NULL;
        query->next_sibling   = ASRTL_PARAM_NONE_ID;
        client->pending       = ASRTR_PARAM_CLIENT_PENDING_DELIVER;
        return ASRTL_SUCCESS;
}

void asrtr_param_client_deinit( struct asrtr_param_client* client )
{
        (void) client;
}
