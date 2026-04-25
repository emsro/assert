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

static enum asrtl_status asrtc_param_server_send( void* p, struct asrtl_rec_span* buff )
{
        struct asrtc_param_server* param = (struct asrtc_param_server*) p;
        return asrtl_send( &param->sendr, ASRTL_PARA, buff, NULL, NULL );
}

static enum asrtl_status asrtc_param_server_handle_ready_ack(
    struct asrtc_param_server* param,
    struct asrtl_span*         buff )
{
        if ( param->pending != ASRTC_PARAM_SERVER_PENDING_NONE ) {
                ASRTL_ERR_LOG( "asrtc_param_server", "ready_ack: pending event not consumed" );
                return ASRTL_RECV_ERR;
        }
        if ( asrtl_span_unfit_for( buff, 4 ) ) {
                ASRTL_ERR_LOG( "asrtc_param_server", "ready_ack: message too short" );
                return ASRTL_RECV_ERR;
        }
        uint32_t max_msg_size;
        asrtl_cut_u32( &buff->b, &max_msg_size );

        param->pending                   = ASRTC_PARAM_SERVER_PENDING_READY_ACK;
        param->pending_data.max_msg_size = max_msg_size;
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtc_param_server_handle_query(
    struct asrtc_param_server* param,
    struct asrtl_span*         buff )
{
        if ( param->pending != ASRTC_PARAM_SERVER_PENDING_NONE ) {
                ASRTL_ERR_LOG( "asrtc_param_server", "query: pending event not consumed" );
                return ASRTL_RECV_ERR;
        }
        if ( !param->ack_received ) {
                ASRTL_ERR_LOG( "asrtc_param_server", "query: ready_ack not received yet" );
                return ASRTL_RECV_ERR;
        }
        if ( !param->tree || !param->enc_buff ) {
                ASRTL_ERR_LOG( "asrtc_param_server", "query: tree or enc_buff not set" );
                return ASRTL_RECV_ERR;
        }
        if ( asrtl_span_unfit_for( buff, 4 ) ) {
                ASRTL_ERR_LOG( "asrtc_param_server", "query: message too short" );
                return ASRTL_RECV_ERR;
        }

        asrt_flat_id node_id;
        asrtl_cut_u32( &buff->b, &node_id );

        param->pending              = ASRTC_PARAM_SERVER_PENDING_QUERY;
        param->pending_data.node_id = node_id;
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtc_param_server_handle_find_by_key(
    struct asrtc_param_server* param,
    struct asrtl_span*         buff )
{
        if ( param->pending != ASRTC_PARAM_SERVER_PENDING_NONE ) {
                ASRTL_ERR_LOG( "asrtc_param_server", "find_by_key: pending event not consumed" );
                return ASRTL_RECV_ERR;
        }
        if ( !param->ack_received ) {
                ASRTL_ERR_LOG( "asrtc_param_server", "find_by_key: ready_ack not received yet" );
                return ASRTL_RECV_ERR;
        }
        if ( !param->tree || !param->enc_buff ) {
                ASRTL_ERR_LOG( "asrtc_param_server", "find_by_key: tree or enc_buff not set" );
                return ASRTL_RECV_ERR;
        }
        if ( asrtl_span_unfit_for( buff, 5 ) ) {
                ASRTL_ERR_LOG( "asrtc_param_server", "find_by_key: message too short" );
                return ASRTL_RECV_ERR;
        }

        asrt_flat_id parent_id;
        asrtl_cut_u32( &buff->b, &parent_id );

        size_t   search_len = (size_t) ( buff->e - buff->b );
        uint8_t* nul        = (uint8_t*) memchr( buff->b, '\0', search_len );
        if ( !nul ) {
                ASRTL_ERR_LOG( "asrtc_param_server", "find_by_key: missing key terminator" );
                return ASRTL_RECV_ERR;
        }
        char const* key = (char const*) buff->b;

        struct asrtl_flat_query_result qr;
        enum asrtl_status              st = asrtl_flat_tree_find_by_key(
            (struct asrtl_flat_tree*) param->tree, parent_id, key, &qr );
        if ( st != ASRTL_SUCCESS ) {
                return asrtl_msg_ctor_param_error(
                    ASRTL_PARAM_ERR_INVALID_QUERY, parent_id, asrtc_param_server_send, param );
        }

        param->pending              = ASRTC_PARAM_SERVER_PENDING_QUERY;
        param->pending_data.node_id = qr.id;
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtc_param_server_tick( struct asrtc_param_server* param, uint32_t now )
{
        switch ( param->pending ) {
        case ASRTC_PARAM_SERVER_PENDING_NONE:
                if ( param->ack_cb && !param->ack_received ) {
                        if ( param->deadline == 0 )
                                param->deadline = now + param->timeout;
                        if ( now >= param->deadline ) {
                                asrtc_param_ready_ack_cb cb = param->ack_cb;
                                void*                    p  = param->ack_cb_ptr;
                                param->ack_cb               = NULL;
                                param->ack_cb_ptr           = NULL;
                                cb( p, ASRTL_TIMEOUT_ERR );
                        }
                }
                return ASRTL_SUCCESS;

        case ASRTC_PARAM_SERVER_PENDING_READY_ACK: {
                uint32_t max_msg_size = param->pending_data.max_msg_size;
                param->pending        = ASRTC_PARAM_SERVER_PENDING_NONE;

                if ( param->enc_buff )
                        asrtl_free( &param->alloc, (void**) &param->enc_buff );
                param->enc_buff = asrtl_alloc( &param->alloc, max_msg_size );
                if ( !param->enc_buff ) {
                        ASRTL_ERR_LOG(
                            "asrtc_param_server",
                            "ready_ack: allocation failed (size=%u)",
                            max_msg_size );
                        return ASRTL_ALLOC_ERR;
                }

                param->max_msg_size = max_msg_size;
                param->ack_received = 1;
                if ( param->ack_cb ) {
                        asrtc_param_ready_ack_cb cb = param->ack_cb;
                        void*                    p  = param->ack_cb_ptr;
                        param->ack_cb               = NULL;
                        param->ack_cb_ptr           = NULL;
                        cb( p, ASRTL_SUCCESS );
                }
                return ASRTL_SUCCESS;
        }

        case ASRTC_PARAM_SERVER_PENDING_QUERY: {
                asrt_flat_id node_id = param->pending_data.node_id;
                param->pending       = ASRTC_PARAM_SERVER_PENDING_NONE;

                uint32_t          out_len;
                enum asrtl_status st = asrtl_msg_ctor_param_response(
                    (struct asrtl_flat_tree*) param->tree,
                    node_id,
                    param->max_msg_size,
                    param->enc_buff,
                    &out_len );

                if ( st == ASRTL_SIZE_ERR ) {
                        ASRTL_ERR_LOG(
                            "asrtc_param_server",
                            "query: response too large for node %u",
                            node_id );
                        return asrtl_msg_ctor_param_error(
                            ASRTL_PARAM_ERR_RESPONSE_TOO_LARGE,
                            node_id,
                            asrtc_param_server_send,
                            param );
                }
                if ( st != ASRTL_SUCCESS ) {
                        ASRTL_ERR_LOG(
                            "asrtc_param_server", "query: encode failure for node %u", node_id );
                        return asrtl_msg_ctor_param_error(
                            ASRTL_PARAM_ERR_ENCODE_FAILURE,
                            node_id,
                            asrtc_param_server_send,
                            param );
                }

                struct asrtl_rec_span span = {
                    .b    = param->enc_buff,
                    .e    = param->enc_buff + out_len,
                    .next = NULL,
                };
                return asrtl_send( &param->sendr, ASRTL_PARA, &span, NULL, NULL );
        }
        }
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtc_param_server_recv( void* data, struct asrtl_span buff )
{
        struct asrtc_param_server* param = (struct asrtc_param_server*) data;

        asrtl_param_message_id id;
        if ( asrtl_span_unfit_for( &buff, sizeof( id ) ) )
                return ASRTL_SUCCESS;
        id = (asrtl_param_message_id) *buff.b++;

        switch ( id ) {
        case ASRTL_PARAM_MSG_READY_ACK:
                return asrtc_param_server_handle_ready_ack( param, &buff );
        case ASRTL_PARAM_MSG_QUERY:
                return asrtc_param_server_handle_query( param, &buff );
        case ASRTL_PARAM_MSG_FIND_BY_KEY:
                return asrtc_param_server_handle_find_by_key( param, &buff );
        default:
                ASRTL_ERR_LOG( "asrtc_param_server", "Unknown param message id: %u", id );
                return ASRTL_RECV_UNEXPECTED_ERR;
        }
}

static enum asrtl_status asrtc_param_server_event( void* p, enum asrtl_event_e e, void* arg )
{
        struct asrtc_param_server* param = (struct asrtc_param_server*) p;
        switch ( e ) {
        case ASRTL_EVENT_TICK:
                return asrtc_param_server_tick( param, *(uint32_t*) arg );
        case ASRTL_EVENT_RECV:
                return asrtc_param_server_recv( param, *(struct asrtl_span*) arg );
        }
        ASRTL_ERR_LOG( "asrtc_param_server", "unexpected event: %s", asrtl_event_to_str( e ) );
        return ASRTL_INVALID_EVENT_ERR;
}

enum asrtl_status asrtc_param_server_init(
    struct asrtc_param_server* param,
    struct asrtl_node*         prev,
    struct asrtl_sender        sender,
    struct asrtl_allocator     alloc )
{
        if ( !param || !prev )
                return ASRTL_INIT_ERR;
        *param = ( struct asrtc_param_server ){
            .node =
                ( struct asrtl_node ){
                    .chid     = ASRTL_PARA,
                    .e_cb_ptr = param,
                    .e_cb     = asrtc_param_server_event,
                    .next     = NULL,
                },
            .sendr        = sender,
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
        asrtl_node_link( prev, &param->node );
        return ASRTL_SUCCESS;
}

void asrtc_param_server_set_tree(
    struct asrtc_param_server*    param,
    struct asrtl_flat_tree const* tree )
{
        param->tree = tree;
}

enum asrtl_status asrtc_param_server_send_ready(
    struct asrtc_param_server* param,
    asrt_flat_id               root_id,
    uint32_t                   timeout,
    asrtc_param_ready_ack_cb   ack_cb,
    void*                      ack_cb_ptr )
{
        if ( !param || !param->tree ) {
                ASRTL_ERR_LOG( "asrtc_param_server", "send_ready: param or tree is NULL" );
                return ASRTL_ARG_ERR;
        }
        struct asrtl_flat_query_result qr;
        if ( asrtl_flat_tree_query( (struct asrtl_flat_tree*) param->tree, root_id, &qr ) !=
             ASRTL_SUCCESS ) {
                ASRTL_ERR_LOG(
                    "asrtc_param_server", "send_ready: root_id %u not found in tree", root_id );
                return ASRTL_ARG_ERR;
        }

        param->ack_received = 0;
        param->max_msg_size = 0;
        param->ack_cb       = ack_cb;
        param->ack_cb_ptr   = ack_cb_ptr;
        param->timeout      = timeout;
        param->deadline     = 0;
        return asrtl_msg_ctor_param_ready( root_id, asrtc_param_server_send, param );
}

void asrtc_param_server_deinit( struct asrtc_param_server* param )
{
        if ( !param )
                return;
        asrtl_node_unlink( &param->node );
        if ( param->enc_buff )
                asrtl_free( &param->alloc, (void**) &param->enc_buff );
}
