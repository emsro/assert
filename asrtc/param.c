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


static enum asrt_status asrt_param_server_handle_ready_ack(
    struct asrt_param_server* param,
    struct asrt_span*         buff )
{
        if ( param->pending != ASRT_PARAM_SERVER_PENDING_NONE ) {
                ASRT_ERR_LOG( "asrt_param_server", "ready_ack: pending event not consumed" );
                return ASRT_RECV_ERR;
        }
        if ( asrt_span_unfit_for( buff, 4 ) ) {
                ASRT_ERR_LOG( "asrt_param_server", "ready_ack: message too short" );
                return ASRT_RECV_ERR;
        }
        uint32_t max_msg_size;
        asrt_cut_u32( &buff->b, &max_msg_size );

        param->pending                   = ASRT_PARAM_SERVER_PENDING_READY_ACK;
        param->pending_data.max_msg_size = max_msg_size;
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_param_server_handle_query(
    struct asrt_param_server* param,
    struct asrt_span*         buff )
{
        if ( param->pending != ASRT_PARAM_SERVER_PENDING_NONE ) {
                ASRT_ERR_LOG( "asrt_param_server", "query: pending event not consumed" );
                return ASRT_RECV_ERR;
        }
        if ( !param->ack_received ) {
                ASRT_ERR_LOG( "asrt_param_server", "query: ready_ack not received yet" );
                return ASRT_RECV_ERR;
        }
        if ( !param->tree || !param->enc_buff ) {
                ASRT_ERR_LOG( "asrt_param_server", "query: tree or enc_buff not set" );
                return ASRT_RECV_ERR;
        }
        if ( asrt_span_unfit_for( buff, 4 ) ) {
                ASRT_ERR_LOG( "asrt_param_server", "query: message too short" );
                return ASRT_RECV_ERR;
        }

        asrt_flat_id node_id;
        asrt_cut_u32( &buff->b, &node_id );

        param->pending              = ASRT_PARAM_SERVER_PENDING_QUERY;
        param->pending_data.node_id = node_id;
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_param_server_handle_find_by_key(
    struct asrt_param_server* param,
    struct asrt_span*         buff )
{
        if ( param->pending != ASRT_PARAM_SERVER_PENDING_NONE ) {
                ASRT_ERR_LOG( "asrt_param_server", "find_by_key: pending event not consumed" );
                return ASRT_RECV_ERR;
        }
        if ( !param->ack_received ) {
                ASRT_ERR_LOG( "asrt_param_server", "find_by_key: ready_ack not received yet" );
                return ASRT_RECV_ERR;
        }
        if ( !param->tree || !param->enc_buff ) {
                ASRT_ERR_LOG( "asrt_param_server", "find_by_key: tree or enc_buff not set" );
                return ASRT_RECV_ERR;
        }
        if ( asrt_span_unfit_for( buff, 5 ) ) {
                ASRT_ERR_LOG( "asrt_param_server", "find_by_key: message too short" );
                return ASRT_RECV_ERR;
        }

        asrt_flat_id parent_id;
        asrt_cut_u32( &buff->b, &parent_id );

        size_t   search_len = (size_t) ( buff->e - buff->b );
        uint8_t* nul        = (uint8_t*) memchr( buff->b, '\0', search_len );
        if ( !nul ) {
                ASRT_ERR_LOG( "asrt_param_server", "find_by_key: missing key terminator" );
                return ASRT_RECV_ERR;
        }
        char const* key = (char const*) buff->b;

        struct asrt_flat_query_result qr;
        enum asrt_status              st =
            asrt_flat_tree_find_by_key( (struct asrt_flat_tree*) param->tree, parent_id, key, &qr );
        if ( st != ASRT_SUCCESS ) {
                if ( asrt_send_is_req_used(
                         param->node.send_queue, &param->find_by_key_err_msg.req ) ) {
                        return ASRT_SUCCESS;  // already sending an error, avoid flooding
                }
                asrt_send_enque(
                    &param->node,
                    asrt_msg_ctor_param_error(
                        &param->find_by_key_err_msg, ASRT_PARAM_ERR_INVALID_QUERY, parent_id ),
                    NULL,
                    NULL );
                return ASRT_SUCCESS;
        }

        param->pending              = ASRT_PARAM_SERVER_PENDING_QUERY;
        param->pending_data.node_id = qr.id;
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_param_server_tick( struct asrt_param_server* param, uint32_t now )
{
        switch ( param->pending ) {
        case ASRT_PARAM_SERVER_PENDING_NONE:
                if ( param->ack_cb && !param->ack_received ) {
                        if ( param->deadline == 0 )
                                param->deadline = now + param->timeout;
                        if ( now >= param->deadline ) {
                                asrt_param_ready_ack_cb cb = param->ack_cb;
                                void*                   p  = param->ack_cb_ptr;
                                param->ack_cb              = NULL;
                                param->ack_cb_ptr          = NULL;
                                cb( p, ASRT_TIMEOUT_ERR );
                        }
                }
                return ASRT_SUCCESS;

        case ASRT_PARAM_SERVER_PENDING_READY_ACK: {
                uint32_t max_msg_size = param->pending_data.max_msg_size;
                param->pending        = ASRT_PARAM_SERVER_PENDING_NONE;

                if ( param->enc_buff )
                        asrt_free( &param->alloc, (void**) &param->enc_buff );
                param->enc_buff = asrt_alloc( &param->alloc, max_msg_size );
                if ( !param->enc_buff ) {
                        ASRT_ERR_LOG(
                            "asrt_param_server",
                            "ready_ack: allocation failed (size=%u)",
                            max_msg_size );
                        return ASRT_ALLOC_ERR;
                }

                param->max_msg_size = max_msg_size;
                param->ack_received = 1;
                if ( param->ack_cb ) {
                        asrt_param_ready_ack_cb cb = param->ack_cb;
                        void*                   p  = param->ack_cb_ptr;
                        param->ack_cb              = NULL;
                        param->ack_cb_ptr          = NULL;
                        cb( p, ASRT_SUCCESS );
                }
                return ASRT_SUCCESS;
        }

        case ASRT_PARAM_SERVER_PENDING_QUERY: {
                asrt_flat_id node_id = param->pending_data.node_id;
                param->pending       = ASRT_PARAM_SERVER_PENDING_NONE;

                uint32_t         out_len;
                enum asrt_status st = asrt_msg_ctor_param_response(
                    (struct asrt_flat_tree*) param->tree,
                    node_id,
                    param->max_msg_size,
                    param->enc_buff,
                    &out_len );

                if ( st == ASRT_SIZE_ERR ) {
                        ASRT_ERR_LOG(
                            "asrt_param_server", "query: response too large for node %u", node_id );

                        if ( asrt_send_is_req_used(
                                 param->node.send_queue, &param->query_err_msg.req ) ) {
                                return ASRT_SUCCESS;  // already sending an error, avoid flooding
                        }
                        asrt_send_enque(
                            &param->node,
                            asrt_msg_ctor_param_error(
                                &param->query_err_msg, ASRT_PARAM_ERR_RESPONSE_TOO_LARGE, node_id ),
                            NULL,
                            NULL );
                        return ASRT_SUCCESS;
                }
                if ( st != ASRT_SUCCESS ) {
                        ASRT_ERR_LOG(
                            "asrt_param_server", "query: encode failure for node %u", node_id );
                        if ( asrt_send_is_req_used(
                                 param->node.send_queue, &param->query_err_msg.req ) ) {
                                return ASRT_SUCCESS;  // already sending an error, avoid flooding
                        }
                        asrt_send_enque(
                            &param->node,
                            asrt_msg_ctor_param_error(
                                &param->query_err_msg, ASRT_PARAM_ERR_ENCODE_FAILURE, node_id ),
                            NULL,
                            NULL );
                        return ASRT_SUCCESS;
                }

                param->query_msg.buff = ( struct asrt_span_span ){
                    .b          = param->enc_buff,
                    .e          = param->enc_buff + out_len,
                    .rest       = NULL,
                    .rest_count = 0 };
                asrt_send_enque( &param->node, &param->query_msg, NULL, NULL );
                return ASRT_SUCCESS;
        }
        }
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_param_server_recv( void* data, struct asrt_span buff )
{
        struct asrt_param_server* param = (struct asrt_param_server*) data;

        asrt_param_message_id id;
        if ( asrt_span_unfit_for( &buff, sizeof( id ) ) )
                return ASRT_SUCCESS;
        id = (asrt_param_message_id) *buff.b++;

        switch ( id ) {
        case ASRT_PARAM_MSG_READY_ACK:
                return asrt_param_server_handle_ready_ack( param, &buff );
        case ASRT_PARAM_MSG_QUERY:
                return asrt_param_server_handle_query( param, &buff );
        case ASRT_PARAM_MSG_FIND_BY_KEY:
                return asrt_param_server_handle_find_by_key( param, &buff );
        default:
                ASRT_ERR_LOG( "asrt_param_server", "Unknown param message id: %u", id );
                return ASRT_RECV_UNEXPECTED_ERR;
        }
}

static enum asrt_status asrt_param_server_event( void* p, enum asrt_event_e e, void* arg )
{
        struct asrt_param_server* param = (struct asrt_param_server*) p;
        switch ( e ) {
        case ASRT_EVENT_TICK:
                return asrt_param_server_tick( param, *(uint32_t*) arg );
        case ASRT_EVENT_RECV:
                return asrt_param_server_recv( param, *(struct asrt_span*) arg );
        }
        ASRT_ERR_LOG( "asrt_param_server", "unexpected event: %s", asrt_event_to_str( e ) );
        return ASRT_INVALID_EVENT_ERR;
}

enum asrt_status asrt_param_server_init(
    struct asrt_param_server* param,
    struct asrt_node*         prev,
    struct asrt_allocator     alloc )
{
        if ( !param || !prev )
                return ASRT_INIT_ERR;
        *param = ( struct asrt_param_server ){
            .node =
                ( struct asrt_node ){
                    .chid       = ASRT_PARA,
                    .e_cb_ptr   = param,
                    .e_cb       = asrt_param_server_event,
                    .next       = NULL,
                    .send_queue = prev->send_queue,
                },
            .tree         = NULL,
            .alloc        = alloc,
            .max_msg_size = 0,
            .ack_received = 0,
            .enc_buff     = NULL,
            .ack_cb       = NULL,
            .ack_cb_ptr   = NULL,
            .timeout      = 0,
            .deadline     = 0,
        };
        asrt_node_link( prev, &param->node );
        return ASRT_SUCCESS;
}

void asrt_param_server_set_tree(
    struct asrt_param_server*    param,
    struct asrt_flat_tree const* tree )
{
        param->tree = tree;
}

enum asrt_status asrt_param_server_send_ready(
    struct asrt_param_server* param,
    asrt_flat_id              root_id,
    uint32_t                  timeout,
    asrt_param_ready_ack_cb   ack_cb,
    void*                     ack_cb_ptr )
{
        if ( !param || !param->tree ) {
                ASRT_ERR_LOG( "asrt_param_server", "send_ready: param or tree is NULL" );
                return ASRT_ARG_ERR;
        }
        struct asrt_flat_query_result qr;
        if ( asrt_flat_tree_query( (struct asrt_flat_tree*) param->tree, root_id, &qr ) !=
             ASRT_SUCCESS ) {
                ASRT_ERR_LOG(
                    "asrt_param_server", "send_ready: root_id %u not found in tree", root_id );
                return ASRT_ARG_ERR;
        }

        param->ack_received = 0;
        param->max_msg_size = 0;
        param->ack_cb       = ack_cb;
        param->ack_cb_ptr   = ack_cb_ptr;
        param->timeout      = timeout;
        param->deadline     = 0;

        asrt_send_enque(
            &param->node, asrt_msg_ctor_param_ready( &param->ready_msg, root_id ), NULL, NULL );

        return ASRT_SUCCESS;
}

void asrt_param_server_deinit( struct asrt_param_server* param )
{
        if ( !param )
                return;
        asrt_node_unlink( &param->node );
        if ( param->enc_buff )
                asrt_free( &param->alloc, (void**) &param->enc_buff );
}
