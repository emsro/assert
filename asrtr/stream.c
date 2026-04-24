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
#include "./stream.h"

#include "../asrtl/log.h"


static void asrtr_stream_send_done( void* ptr, enum asrtl_status status )
{
        struct asrtr_stream_client* client = (struct asrtr_stream_client*) ptr;
        client->op.done.send_status        = status;
        if ( client->state != ASRTR_STRM_ERROR )
                client->state = ASRTR_STRM_DONE;
}

static inline enum asrtl_status asrtr_stream_send( void* p, struct asrtl_rec_span* buff )
{
        struct asrtr_stream_client* client = (struct asrtr_stream_client*) p;
        return asrtl_send( &client->sendr, ASRTL_STRM, buff, asrtr_stream_send_done, client );
}

static enum asrtl_status asrtr_stream_client_recv( void* data, struct asrtl_span buff )
{
        struct asrtr_stream_client* client = (struct asrtr_stream_client*) data;

        if ( asrtl_span_unfit_for( &buff, 1 ) )
                return ASRTL_SUCCESS;
        asrtl_strm_message_id id = (asrtl_strm_message_id) *buff.b++;

        switch ( id ) {
        case ASRTL_STRM_MSG_ERROR:
                if ( asrtl_span_unfit_for( &buff, 1 ) ) {
                        ASRTL_ERR_LOG( "asrtr_stream", "error message too short" );
                        return ASRTL_RECV_ERR;
                }
                client->err_code = ( enum asrtl_strm_err_e ) * buff.b++;
                client->state    = ASRTR_STRM_ERROR;
                ASRTL_ERR_LOG( "asrtr_stream", "error from controller: code=%u", client->err_code );
                return ASRTL_SUCCESS;
        default:
                ASRTL_ERR_LOG( "asrtr_stream", "unexpected message id: %u", id );
                return ASRTL_RECV_UNEXPECTED_ERR;
        }
}


enum asrtl_status asrtr_stream_client_define(
    struct asrtr_stream_client*         client,
    uint8_t                             schema_id,
    enum asrtl_strm_field_type_e const* fields,
    uint8_t                             field_count,
    asrtr_stream_done_cb                done_cb,
    void*                               done_cb_ptr )
{
        if ( !client || !fields || field_count == 0 )
                return ASRTL_ARG_ERR;
        if ( client->state != ASRTR_STRM_IDLE )
                return ASRTL_BUSY_ERR;

        client->op.define = ( struct asrtr_stream_pending_define ){
            .schema_id   = schema_id,
            .field_count = field_count,
            .fields      = fields,
        };
        client->done_cb     = done_cb;
        client->done_cb_ptr = done_cb_ptr;
        client->state       = ASRTR_STRM_DEFINE_SEND;

        return ASRTL_SUCCESS;
}

enum asrtl_status asrtr_stream_client_emit(
    struct asrtr_stream_client* client,
    uint8_t                     schema_id,
    uint8_t const*              data,
    uint16_t                    data_size,
    asrtr_stream_done_cb        done_cb,
    void*                       done_cb_ptr )
{
        if ( !client || !data )
                return ASRTL_ARG_ERR;
        if ( client->state == ASRTR_STRM_ERROR )
                return ASRTL_INTERNAL_ERR;
        if ( client->state != ASRTR_STRM_IDLE )
                return ASRTL_BUSY_ERR;

        client->done_cb     = done_cb;
        client->done_cb_ptr = done_cb_ptr;
        client->state       = ASRTR_STRM_WAIT;

        enum asrtl_status st =
            asrtl_msg_rtoc_strm_data( schema_id, data, data_size, asrtr_stream_send, client );
        if ( st != ASRTL_SUCCESS ) {
                client->done_cb     = NULL;
                client->done_cb_ptr = NULL;
                client->state       = ASRTR_STRM_IDLE;
                return ASRTL_SEND_ERR;
        }
        return ASRTL_SUCCESS;
}

enum asrtl_status asrtr_stream_client_reset( struct asrtr_stream_client* client )
{
        if ( !client )
                return ASRTL_INIT_ERR;
        if ( client->state == ASRTR_STRM_DEFINE_SEND || client->state == ASRTR_STRM_WAIT )
                return ASRTL_BUSY_ERR;
        client->state       = ASRTR_STRM_IDLE;
        client->err_code    = ASRTL_STRM_ERR_SUCCESS;
        client->done_cb     = NULL;
        client->done_cb_ptr = NULL;
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtr_stream_tick_done( struct asrtr_stream_client* client )
{
        asrtr_stream_done_cb cb     = client->done_cb;
        void*                cb_ptr = client->done_cb_ptr;
        enum asrtl_status    st     = client->op.done.send_status;

        client->done_cb     = NULL;
        client->done_cb_ptr = NULL;
        client->state       = ASRTR_STRM_IDLE;

        if ( cb )
                cb( cb_ptr, st );

        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtr_stream_tick_define_send( struct asrtr_stream_client* client )
{
        client->state = ASRTR_STRM_WAIT;

        struct asrtr_stream_pending_define* p = &client->op.define;

        enum asrtl_status st = asrtl_msg_rtoc_strm_define(
            p->schema_id, p->fields, p->field_count, asrtr_stream_send, client );
        if ( st != ASRTL_SUCCESS ) {
                ASRTL_ERR_LOG(
                    "asrtr_stream_client",
                    "failed to send DEFINE message: %s",
                    asrtl_status_to_str( st ) );
                return ASRTL_SEND_ERR;
        }
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtr_stream_client_tick( struct asrtr_stream_client* client )
{
        if ( !client )
                return ASRTL_INTERNAL_ERR;

        switch ( client->state ) {
        case ASRTR_STRM_IDLE:
        case ASRTR_STRM_WAIT:
        case ASRTR_STRM_ERROR:
                break;
        case ASRTR_STRM_DEFINE_SEND:
                return asrtr_stream_tick_define_send( client );
        case ASRTR_STRM_DONE:
                return asrtr_stream_tick_done( client );
        }

        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtr_stream_client_event( void* p, enum asrtl_event_e e, void* arg )
{
        struct asrtr_stream_client* client = (struct asrtr_stream_client*) p;
        switch ( e ) {
        case ASRTL_EVENT_TICK:
                return asrtr_stream_client_tick( client );
        case ASRTL_EVENT_RECV:
                return asrtr_stream_client_recv( client, *(struct asrtl_span*) arg );
        }
        ASRTL_ERR_LOG( "asrtr_stream_client", "unexpected event: %s", asrtl_event_to_str( e ) );
        return ASRTL_INVALID_EVENT_ERR;
}

enum asrtl_status asrtr_stream_client_init(
    struct asrtr_stream_client* client,
    struct asrtl_node*          prev,
    struct asrtl_sender         sender )
{
        if ( !client || !prev )
                return ASRTL_INIT_ERR;
        *client = ( struct asrtr_stream_client ){
            .node =
                ( struct asrtl_node ){
                    .chid     = ASRTL_STRM,
                    .e_cb_ptr = client,
                    .e_cb     = asrtr_stream_client_event,
                    .next     = NULL,
                },
            .sendr = sender,
            .state = ASRTR_STRM_IDLE,
        };
        asrtl_node_link( prev, &client->node );
        return ASRTL_SUCCESS;
}

void asrtr_stream_client_deinit( struct asrtr_stream_client* client )
{
        if ( !client )
                return;
        asrtl_node_unlink( &client->node );
}
