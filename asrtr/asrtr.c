
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
#include "../asrtl/asrt_assert.h"
#include "../asrtl/core_proto.h"
#include "../asrtl/diag_proto.h"
#include "../asrtl/log.h"
#include "../asrtl/proto_version.h"
#include "../asrtl/status_to_str.h"
#include "./diag.h"
#include "./reactor.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

void asrt_fail( struct asrt_record* rec )
{
        rec->state = ASRT_TEST_FAIL;
}


static enum asrt_status asrt_diag_event( void* p, enum asrt_event_e e, void* arg )
{
        (void) arg;
        (void) p;
        switch ( e ) {
        case ASRT_EVENT_TICK:
                return ASRT_SUCCESS;
        case ASRT_EVENT_RECV:
                ASRT_ERR_LOG( "asrt_diag", "Received unexpected message on diag channel" );
                return ASRT_INVALID_EVENT_ERR;
        }
        return ASRT_SUCCESS;
}

enum asrt_status asrt_diag_client_init( struct asrt_diag_client* diag, struct asrt_node* prev )
{
        if ( !diag || !prev ) {
                ASRT_ERR_LOG( "asrt_diag", "Invalid arguments to diag init" );
                return ASRT_INIT_ERR;
        }
        *diag = ( struct asrt_diag_client ){
            .node =
                ( struct asrt_node ){
                    .chid       = ASRT_DIAG,
                    .e_cb_ptr   = diag,
                    .e_cb       = asrt_diag_event,
                    .next       = NULL,
                    .send_queue = prev->send_queue,
                },
        };
        asrt_node_link( prev, &diag->node );
        return ASRT_SUCCESS;
}

void asrt_diag_client_deinit( struct asrt_diag_client* diag )
{
        if ( !diag )
                return;
        asrt_node_unlink( &diag->node );
}

enum asrt_diag_record_result asrt_diag_client_record(
    struct asrt_diag_client* diag,
    char const*              file,
    uint32_t                 line,
    char const*              extra,
    asrt_diag_record_done_cb done_cb,
    void*                    done_ptr )
{
        ASRT_ASSERT( diag );
        ASRT_ASSERT( file );

        ASRT_INF_LOG( "asrt_diag", "Sending diag message: %s:%u", file, line );

        if ( asrt_send_is_req_used( diag->node.send_queue, &diag->msg.req ) ) {
                ASRT_ERR_LOG( "asrt_diag", "diag slot busy, dropping record" );
                return ASRT_DIAG_RECORD_BUSY;
        }
        asrt_send_enque(
            &diag->node,
            asrt_msg_rtoc_diag_record( &diag->msg, file, line, extra ),
            done_cb,
            done_ptr );
        return ASRT_DIAG_RECORD_ACCEPTED;
}

static enum asrt_status asrt_send_test_start_error(
    struct asrt_reactor* reac,
    uint16_t             test_id,
    uint32_t             run_id );

static void asrt_reactor_wait_send_done( void* ptr, enum asrt_status st )
{
        struct asrt_reactor* reac = (struct asrt_reactor*) ptr;
        ASRT_ASSERT( reac->state == ASRT_REAC_WAIT_SEND );
        if ( st == ASRT_SUCCESS ) {
                reac->state = reac->wait_send.next_state;
                return;
        }
        ASRT_ERR_LOG( "asrtr_asrtr", "waited send failed, sending error result" );
        if ( asrt_send_is_req_used( reac->node.send_queue, &reac->wait_send.err_result_msg.req ) ) {
                ASRT_ERR_LOG( "asrtr_asrtr", "error result slot busy, dropping test error" );
                reac->state = ASRT_REAC_IDLE;
                return;
        }
        asrt_send_enque(
            &reac->node,
            asrt_msg_rtoc_test_result(
                &reac->wait_send.err_result_msg,
                reac->wait_send.err_run_id,
                ASRT_TEST_RESULT_ERROR ),
            NULL,
            NULL );
        reac->state = ASRT_REAC_IDLE;
}

static void asrt_reactor_error_start_sent( void* ptr, enum asrt_status st )
{
        struct asrt_reactor* reac = (struct asrt_reactor*) ptr;
        ASRT_ASSERT( reac->state == ASRT_REAC_WAIT_SEND );
        if ( st != ASRT_SUCCESS ) {
                ASRT_ERR_LOG( "asrtr_asrtr", "error start send failed, skipping result" );
                reac->state = reac->wait_send.next_state;
                return;
        }
        ASRT_ASSERT( !asrt_send_is_req_used( reac->node.send_queue, &reac->test_result_msg.req ) );
        asrt_send_enque(
            &reac->node,
            asrt_msg_rtoc_test_result(
                &reac->test_result_msg, reac->wait_send.err_run_id, ASRT_TEST_RESULT_ERROR ),
            asrt_reactor_wait_send_done,
            reac );
}

static enum asrt_status asrt_send_test_start_error(
    struct asrt_reactor* reac,
    uint16_t             test_id,
    uint32_t             run_id )
{
        ASRT_ASSERT( !asrt_send_is_req_used( reac->node.send_queue, &reac->test_start_msg.req ) );
        ASRT_ASSERT( !asrt_send_is_req_used( reac->node.send_queue, &reac->test_result_msg.req ) );
        reac->wait_send.next_state = reac->state;
        reac->wait_send.err_run_id = run_id;
        reac->state                = ASRT_REAC_WAIT_SEND;
        asrt_send_enque(
            &reac->node,
            asrt_msg_rtoc_test_start( &reac->test_start_msg, test_id, run_id ),
            asrt_reactor_error_start_sent,
            reac );
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_reactor_tick_flag_test_info( struct asrt_reactor* reac )
{
        struct asrt_test* t = reac->first_test;
        uint32_t          i = reac->recv_test_info_id;
        while ( i-- > 0 && t )
                t = t->next;
        ASRT_ASSERT( !asrt_send_is_req_used( reac->node.send_queue, &reac->ti_msg.req ) );
        reac->wait_send.next_state = reac->state;
        reac->wait_send.err_run_id = 0;
        reac->state                = ASRT_REAC_WAIT_SEND;
        if ( !t ) {
                asrt_send_enque(
                    &reac->node,
                    asrt_msg_rtoc_test_info(
                        &reac->ti_msg,
                        reac->recv_test_info_id,
                        ASRT_TEST_INFO_MISSING_TEST_ERR,
                        "",
                        0 ),
                    asrt_reactor_wait_send_done,
                    reac );
        } else {
                asrt_send_enque(
                    &reac->node,
                    asrt_msg_rtoc_test_info(
                        &reac->ti_msg,
                        reac->recv_test_info_id,
                        ASRT_TEST_INFO_SUCCESS,
                        t->desc,
                        strlen( t->desc ) ),
                    asrt_reactor_wait_send_done,
                    reac );
        }
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_reactor_tick_flag_test_start( struct asrt_reactor* reac )
{
        if ( reac->state != ASRT_REAC_IDLE ) {
                if ( asrt_send_test_start_error(
                         reac, reac->recv_test_start_id, reac->recv_test_run_id ) !=
                     ASRT_SUCCESS ) {
                        ASRT_ERR_LOG( "asrtr_asrtr", "Failed to send busy test error result" );
                        return ASRT_SEND_ERR;
                }
                return ASRT_SUCCESS;
        }
        struct asrt_test* t = reac->first_test;
        uint32_t          i = reac->recv_test_start_id;
        while ( i-- > 0 && t )
                t = t->next;
        if ( !t ) {
                if ( asrt_send_test_start_error(
                         reac, reac->recv_test_start_id, reac->recv_test_run_id ) !=
                     ASRT_SUCCESS ) {
                        ASRT_ERR_LOG( "asrtr_asrtr", "Failed to send missing test error result" );
                        return ASRT_SEND_ERR;
                }
        } else {
                reac->test_info = ( struct asrt_test_input ){
                    .test_ptr   = t->ptr,
                    .continue_f = t->start_f,
                    .run_id     = reac->recv_test_run_id,
                };
                reac->record = ( struct asrt_record ){
                    .state = ASRT_TEST_INIT,
                    .inpt  = &reac->test_info,
                };
                reac->wait_send.next_state = ASRT_REAC_TEST_EXEC;
                reac->wait_send.err_run_id = reac->recv_test_run_id;
                reac->state                = ASRT_REAC_WAIT_SEND;
                ASRT_ASSERT(
                    !asrt_send_is_req_used( reac->node.send_queue, &reac->test_start_msg.req ) );
                asrt_send_enque(
                    &reac->node,
                    asrt_msg_rtoc_test_start(
                        &reac->test_start_msg, reac->recv_test_start_id, reac->recv_test_run_id ),
                    asrt_reactor_wait_send_done,
                    reac );
        }
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_reactor_tick_flag_desc( struct asrt_reactor* reac )
{
        ASRT_ASSERT( !asrt_send_is_req_used( reac->node.send_queue, &reac->desc_msg.req ) );
        reac->wait_send.next_state = reac->state;
        reac->wait_send.err_run_id = 0;
        reac->state                = ASRT_REAC_WAIT_SEND;
        asrt_send_enque(
            &reac->node,
            asrt_msg_rtoc_desc( &reac->desc_msg, reac->desc, strlen( reac->desc ) ),
            asrt_reactor_wait_send_done,
            reac );
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_reactor_tick_flag_proto_ver( struct asrt_reactor* reac )
{
        ASRT_ASSERT( !asrt_send_is_req_used( reac->node.send_queue, &reac->proto_ver_msg.req ) );
        reac->wait_send.next_state = reac->state;
        reac->wait_send.err_run_id = 0;
        reac->state                = ASRT_REAC_WAIT_SEND;
        asrt_send_enque(
            &reac->node,
            asrt_msg_rtoc_proto_version(
                &reac->proto_ver_msg, ASRT_PROTO_MAJOR, ASRT_PROTO_MINOR, ASRT_PROTO_PATCH ),
            asrt_reactor_wait_send_done,
            reac );
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_reactor_tick_flag_tc( struct asrt_reactor* reac )
{
        uint16_t          count = 0;
        struct asrt_test* t     = reac->first_test;
        while ( t != NULL )
                ++count, t = t->next;
        ASRT_INF_LOG( "asrtr_asrtr", "Sending test count: %u", count );
        ASRT_ASSERT( !asrt_send_is_req_used( reac->node.send_queue, &reac->tc_msg.req ) );
        reac->wait_send.next_state = reac->state;
        reac->wait_send.err_run_id = 0;
        reac->state                = ASRT_REAC_WAIT_SEND;
        asrt_send_enque(
            &reac->node,
            asrt_msg_rtoc_test_count( &reac->tc_msg, count ),
            asrt_reactor_wait_send_done,
            reac );
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_reactor_tick_flags( struct asrt_reactor* reac )
{
        ASRT_ASSERT( reac );
        ASRT_ASSERT( reac->desc );

        uint32_t mask = 0;

        if ( reac->flags & ASRT_FLAG_DESC ) {
                mask = ~ASRT_FLAG_DESC;
                ASRT_INF_LOG( "asrtr_asrtr", "Sending description" );
                if ( asrt_reactor_tick_flag_desc( reac ) != ASRT_SUCCESS )
                        return ASRT_SEND_ERR;
        } else if ( reac->flags & ASRT_FLAG_PROTO_VER ) {
                mask = ~ASRT_FLAG_PROTO_VER;
                ASRT_INF_LOG( "asrtr_asrtr", "Sending protocol version" );
                if ( asrt_reactor_tick_flag_proto_ver( reac ) != ASRT_SUCCESS )
                        return ASRT_SEND_ERR;
        } else if ( reac->flags & ASRT_FLAG_TC ) {
                mask = ~ASRT_FLAG_TC;
                if ( asrt_reactor_tick_flag_tc( reac ) != ASRT_SUCCESS )
                        return ASRT_SEND_ERR;
        } else if ( reac->flags & ASRT_FLAG_TI ) {
                mask = ~ASRT_FLAG_TI;
                ASRT_INF_LOG( "asrtr_asrtr", "Sending test %u info", reac->recv_test_info_id );
                if ( asrt_reactor_tick_flag_test_info( reac ) != ASRT_SUCCESS )
                        return ASRT_SEND_ERR;
        } else if ( reac->flags & ASRT_FLAG_TSTART ) {
                mask = ~ASRT_FLAG_TSTART;
                ASRT_INF_LOG(
                    "asrtr_asrtr",
                    "Starting test %u, run ID: %u",
                    reac->recv_test_start_id,
                    reac->recv_test_run_id );
                if ( asrt_reactor_tick_flag_test_start( reac ) != ASRT_SUCCESS )
                        return ASRT_SEND_ERR;
        } else {
                ASRT_ERR_LOG( "asrtr_asrtr", "Unknown flag bits set: 0x%x", reac->flags );
                reac->flags = 0;
                return ASRT_INTERNAL_ERR;
        }

        if ( mask != 0 )
                reac->flags &= mask;

        return ASRT_SUCCESS;
}

static enum asrt_status asrt_reactor_tick_exec( struct asrt_reactor* reac )
{
        struct asrt_record* record = &reac->record;
        ASRT_ASSERT( record );
        ASRT_ASSERT( record->inpt->continue_f );

        if ( record->inpt->continue_f( record ) != ASRT_SUCCESS )
                record->state = ASRT_TEST_ERROR;

        switch ( record->state ) {
        case ASRT_TEST_INIT:
        case ASRT_TEST_RUNNING:
                break;
        case ASRT_TEST_ERROR:
        case ASRT_TEST_FAIL:
        case ASRT_TEST_PASS:
                reac->state = ASRT_REAC_TEST_REPORT;
                break;
        }
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_reactor_tick_report( struct asrt_reactor* reac )
{
        struct asrt_record* record = &reac->record;
        asrt_test_result    res    = record->state == ASRT_TEST_ERROR ? ASRT_TEST_RESULT_ERROR :
                                     record->state == ASRT_TEST_FAIL  ? ASRT_TEST_RESULT_FAILURE :
                                                                        ASRT_TEST_RESULT_SUCCESS;
        ASRT_ASSERT( !asrt_send_is_req_used( reac->node.send_queue, &reac->test_result_msg.req ) );
        reac->wait_send.next_state = ASRT_REAC_IDLE;
        reac->wait_send.err_run_id = 0;
        reac->state                = ASRT_REAC_WAIT_SEND;
        asrt_send_enque(
            &reac->node,
            asrt_msg_rtoc_test_result( &reac->test_result_msg, record->inpt->run_id, res ),
            asrt_reactor_wait_send_done,
            reac );
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_reactor_tick( struct asrt_reactor* reac )
{
        ASRT_ASSERT( reac );
        ASRT_ASSERT( reac->desc );

        if ( reac->state == ASRT_REAC_WAIT_SEND )
                return ASRT_SUCCESS;

        if ( reac->flags & ~ASRT_PASSIVE_FLAGS )
                return asrt_reactor_tick_flags( reac );

        switch ( reac->state ) {
        case ASRT_REAC_TEST_EXEC:
                return asrt_reactor_tick_exec( reac );
        case ASRT_REAC_TEST_REPORT:
                return asrt_reactor_tick_report( reac );
        case ASRT_REAC_IDLE:
        default:
                break;
        }

        return ASRT_SUCCESS;
}

static enum asrt_status asrt_reactor_recv( void* data, struct asrt_span buff )
{
        ASRT_ASSERT( data );
        struct asrt_reactor* r = (struct asrt_reactor*) data;
        asrt_message_id      id;

        if ( asrt_span_unfit_for( &buff, sizeof( asrt_message_id ) ) ) {
                ASRT_ERR_LOG( "asrtr_asrtr", "Message too short for ID" );
                return ASRT_RECV_ERR;
        }
        asrt_cut_u16( &buff.b, &id );

        enum asrt_message_id_e eid = (enum asrt_message_id_e) id;
        switch ( eid ) {
        case ASRT_MSG_PROTO_VERSION:
                ASRT_INF_LOG( "asrtr_asrtr", "Protocol version requested" );
                r->flags |= ASRT_FLAG_PROTO_VER;
                break;
        case ASRT_MSG_DESC:
                ASRT_INF_LOG( "asrtr_asrtr", "Description requested" );
                r->flags |= ASRT_FLAG_DESC;
                break;
        case ASRT_MSG_TEST_COUNT:
                ASRT_INF_LOG( "asrtr_asrtr", "Test count requested" );
                r->flags |= ASRT_FLAG_TC;
                break;
        case ASRT_MSG_TEST_INFO: {
                if ( r->flags & ASRT_FLAG_TI ) {
                        ASRT_ERR_LOG( "asrtr_asrtr", "TEST_INFO already pending" );
                        return ASRT_RECV_UNEXPECTED_ERR;
                }
                if ( asrt_span_unfit_for( &buff, sizeof( uint16_t ) ) ) {
                        ASRT_ERR_LOG( "asrtr_asrtr", "TEST_INFO message too short" );
                        return ASRT_RECV_ERR;
                }
                asrt_cut_u16( &buff.b, &r->recv_test_info_id );
                ASRT_INF_LOG( "asrtr_asrtr", "Test %i info requested", r->recv_test_info_id );
                r->flags |= ASRT_FLAG_TI;
                break;
        }
        case ASRT_MSG_TEST_START: {
                if ( r->flags & ASRT_FLAG_TSTART ) {
                        ASRT_ERR_LOG( "asrtr_asrtr", "TEST_START already pending" );
                        return ASRT_RECV_UNEXPECTED_ERR;
                }
                if ( asrt_span_unfit_for( &buff, sizeof( uint16_t ) + sizeof( uint32_t ) ) ) {
                        ASRT_ERR_LOG( "asrtr_asrtr", "TEST_START message too short" );
                        return ASRT_RECV_ERR;
                }
                asrt_cut_u16( &buff.b, &r->recv_test_start_id );
                asrt_cut_u32( &buff.b, &r->recv_test_run_id );
                ASRT_INF_LOG(
                    "asrtr_asrtr",
                    "Test %i start requested, run ID: %u",
                    r->recv_test_start_id,
                    r->recv_test_run_id );
                r->flags |= ASRT_FLAG_TSTART;
                break;
        }
        case ASRT_MSG_TEST_RESULT:
        default:
                ASRT_ERR_LOG( "asrtr_asrtr", "Unknown message ID: %u", id );
                return ASRT_RECV_UNKNOWN_ID_ERR;
        }
        // If not all bytes are consumed - error
        enum asrt_status res = buff.b == buff.e ? ASRT_SUCCESS : ASRT_RECV_ERR;
        if ( res == ASRT_RECV_ERR )
                ASRT_ERR_LOG( "asrtr_asrtr", "Unused bytes: %zu", (size_t) ( buff.e - buff.b ) );
        else
                r->flags |= ASRT_FLAG_LOCKED;
        return res;
}

enum asrt_status asrt_test_init(
    struct asrt_test*  t,
    char const*        desc,
    void*              ptr,
    asrt_test_callback start_f )
{
        if ( !t || !desc || !start_f ) {
                ASRT_ERR_LOG( "asrtr_asrtr", "Invalid arguments to test init" );
                return ASRT_INIT_ERR;
        }
        *t = ( struct asrt_test ){
            .desc    = desc,
            .ptr     = ptr,
            .start_f = start_f,
            .next    = NULL,
        };
        return ASRT_SUCCESS;
}

enum asrt_status asrt_reactor_add_test( struct asrt_reactor* reac, struct asrt_test* test )
{
        ASRT_ASSERT( reac );
        ASRT_ASSERT( test );
        if ( reac->flags & ASRT_FLAG_LOCKED ) {
                ASRT_ERR_LOG( "asrtr_asrtr", "Test registration locked after first recv" );
                return ASRT_BUSY_ERR;
        }
        test->next = NULL;
        if ( reac->last_test )
                reac->last_test->next = test;
        else
                reac->first_test = test;
        reac->last_test = test;
        return ASRT_SUCCESS;
}

enum asrt_status asrt_reactor_event( void* p, enum asrt_event_e e, void* arg )
{
        switch ( e ) {
        case ASRT_EVENT_TICK:
                return asrt_reactor_tick( (struct asrt_reactor*) p /*, *(uint32_t*) arg*/ );
        case ASRT_EVENT_RECV:
                return asrt_reactor_recv( p, *(struct asrt_span*) arg );
        }
        ASRT_ERR_LOG( "asrtr_asrtr", "Received unexpected event on reactor channel" );
        return ASRT_INVALID_EVENT_ERR;
}

enum asrt_status asrt_reactor_init(
    struct asrt_reactor*       reac,
    struct asrt_send_req_list* send_queue,
    char const*                desc )
{
        if ( !reac || !desc || !send_queue ) {
                ASRT_ERR_LOG( "asrtr_asrtr", "Invalid arguments to reactor init" );
                return ASRT_INIT_ERR;
        }
        *reac = ( struct asrt_reactor ){
            .node =
                ( struct asrt_node ){
                    .chid       = ASRT_CORE,
                    .e_cb_ptr   = reac,
                    .e_cb       = asrt_reactor_event,
                    .next       = NULL,
                    .send_queue = send_queue,
                },
            .desc               = desc,
            .first_test         = NULL,
            .last_test          = NULL,
            .state              = ASRT_REAC_IDLE,
            .flags              = 0,
            .recv_test_info_id  = 0,
            .recv_test_start_id = 0,
        };
        return ASRT_SUCCESS;
}

void asrt_reactor_deinit( struct asrt_reactor* reac )
{
        if ( !reac )
                return;
        asrt_node_unlink( &reac->node );
}
