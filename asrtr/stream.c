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


static void asrtr_stream_send_done( void* ptr, enum asrt_status status )
{
        struct asrtr_stream_client* client = (struct asrtr_stream_client*) ptr;
        client->op.done.send_status        = status;
        if ( client->state != ASRTR_STRM_ERROR )
                client->state = ASRTR_STRM_DONE;
}

static inline enum asrt_status asrtr_stream_send( void* p, struct asrt_rec_span* buff )
{
        struct asrtr_stream_client* client = (struct asrtr_stream_client*) p;
        return asrt_send( &client->sendr, ASRT_STRM, buff, asrtr_stream_send_done, client );
}

static enum asrt_status asrtr_stream_client_recv( void* data, struct asrt_span buff )
{
        struct asrtr_stream_client* client = (struct asrtr_stream_client*) data;

        if ( asrt_span_unfit_for( &buff, 1 ) )
                return ASRT_SUCCESS;
        asrt_strm_message_id id = (asrt_strm_message_id) *buff.b++;

        switch ( id ) {
        case ASRT_STRM_MSG_ERROR:
                if ( asrt_span_unfit_for( &buff, 1 ) ) {
                        ASRT_ERR_LOG( "asrtr_stream", "error message too short" );
                        return ASRT_RECV_ERR;
                }
                client->err_code = ( enum asrt_strm_err_e ) * buff.b++;
                client->state    = ASRTR_STRM_ERROR;
                ASRT_ERR_LOG( "asrtr_stream", "error from controller: code=%u", client->err_code );
                return ASRT_SUCCESS;
        default:
                ASRT_ERR_LOG( "asrtr_stream", "unexpected message id: %u", id );
                return ASRT_RECV_UNEXPECTED_ERR;
        }
}


enum asrt_status asrtr_stream_client_define(
    struct asrtr_stream_client*        client,
    uint8_t                            schema_id,
    enum asrt_strm_field_type_e const* fields,
    uint8_t                            field_count,
    asrtr_stream_done_cb               done_cb,
    void*                              done_cb_ptr )
{
        if ( !client || !fields || field_count == 0 )
                return ASRT_ARG_ERR;
        if ( client->state != ASRTR_STRM_IDLE )
                return ASRT_BUSY_ERR;

        client->op.define = ( struct asrtr_stream_pending_define ){
            .schema_id   = schema_id,
            .field_count = field_count,
            .fields      = fields,
        };
        client->done_cb     = done_cb;
        client->done_cb_ptr = done_cb_ptr;
        client->state       = ASRTR_STRM_DEFINE_SEND;

        return ASRT_SUCCESS;
}

enum asrt_status asrtr_stream_client_emit(
    struct asrtr_stream_client* client,
    uint8_t                     schema_id,
    uint8_t const*              data,
    uint16_t                    data_size,
    asrtr_stream_done_cb        done_cb,
    void*                       done_cb_ptr )
{
        if ( !client || !data )
                return ASRT_ARG_ERR;
        if ( client->state == ASRTR_STRM_ERROR )
                return ASRT_INTERNAL_ERR;
        if ( client->state != ASRTR_STRM_IDLE )
                return ASRT_BUSY_ERR;

        client->done_cb     = done_cb;
        client->done_cb_ptr = done_cb_ptr;
        client->state       = ASRTR_STRM_WAIT;

        enum asrt_status st =
            asrt_msg_rtoc_strm_data( schema_id, data, data_size, asrtr_stream_send, client );
        if ( st != ASRT_SUCCESS ) {
                client->done_cb     = NULL;
                client->done_cb_ptr = NULL;
                client->state       = ASRTR_STRM_IDLE;
                return ASRT_SEND_ERR;
        }
        return ASRT_SUCCESS;
}

enum asrt_status asrtr_stream_client_reset( struct asrtr_stream_client* client )
{
        if ( !client )
                return ASRT_INIT_ERR;
        if ( client->state == ASRTR_STRM_DEFINE_SEND || client->state == ASRTR_STRM_WAIT )
                return ASRT_BUSY_ERR;
        client->state       = ASRTR_STRM_IDLE;
        client->err_code    = ASRT_STRM_ERR_SUCCESS;
        client->done_cb     = NULL;
        client->done_cb_ptr = NULL;
        return ASRT_SUCCESS;
}

static enum asrt_status asrtr_stream_tick_done( struct asrtr_stream_client* client )
{
        asrtr_stream_done_cb cb     = client->done_cb;
        void*                cb_ptr = client->done_cb_ptr;
        enum asrt_status     st     = client->op.done.send_status;

        client->done_cb     = NULL;
        client->done_cb_ptr = NULL;
        client->state       = ASRTR_STRM_IDLE;

        if ( cb )
                cb( cb_ptr, st );

        return ASRT_SUCCESS;
}

static enum asrt_status asrtr_stream_tick_define_send( struct asrtr_stream_client* client )
{
        client->state = ASRTR_STRM_WAIT;

        struct asrtr_stream_pending_define* p = &client->op.define;

        enum asrt_status st = asrt_msg_rtoc_strm_define(
            p->schema_id, p->fields, p->field_count, asrtr_stream_send, client );
        if ( st != ASRT_SUCCESS ) {
                ASRT_ERR_LOG(
                    "asrtr_stream_client",
                    "failed to send DEFINE message: %s",
                    asrt_status_to_str( st ) );
                return ASRT_SEND_ERR;
        }
        return ASRT_SUCCESS;
}

static enum asrt_status asrtr_stream_client_tick( struct asrtr_stream_client* client )
{
        if ( !client )
                return ASRT_INTERNAL_ERR;

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

        return ASRT_SUCCESS;
}

static enum asrt_status asrtr_stream_client_event( void* p, enum asrt_event_e e, void* arg )
{
        struct asrtr_stream_client* client = (struct asrtr_stream_client*) p;
        switch ( e ) {
        case ASRT_EVENT_TICK:
                return asrtr_stream_client_tick( client );
        case ASRT_EVENT_RECV:
                return asrtr_stream_client_recv( client, *(struct asrt_span*) arg );
        }
        ASRT_ERR_LOG( "asrtr_stream_client", "unexpected event: %s", asrt_event_to_str( e ) );
        return ASRT_INVALID_EVENT_ERR;
}

enum asrt_status asrtr_stream_client_init(
    struct asrtr_stream_client* client,
    struct asrt_node*           prev,
    struct asrt_sender          sender )
{
        if ( !client || !prev )
                return ASRT_INIT_ERR;
        *client = ( struct asrtr_stream_client ){
            .node =
                ( struct asrt_node ){
                    .chid     = ASRT_STRM,
                    .e_cb_ptr = client,
                    .e_cb     = asrtr_stream_client_event,
                    .next     = NULL,
                },
            .sendr = sender,
            .state = ASRTR_STRM_IDLE,
        };
        asrt_node_link( prev, &client->node );
        return ASRT_SUCCESS;
}

void asrtr_stream_client_deinit( struct asrtr_stream_client* client )
{
        if ( !client )
                return;
        asrt_node_unlink( &client->node );
}
