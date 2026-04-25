
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

void asrtr_fail( struct asrtr_record* rec )
{
        rec->state = ASRTR_TEST_FAIL;
}


static enum asrt_status asrtr_diag_event( void* p, enum asrt_event_e e, void* arg )
{
        (void) arg;
        (void) p;
        switch ( e ) {
        case ASRT_EVENT_TICK:
                return ASRT_SUCCESS;
        case ASRT_EVENT_RECV:
                ASRT_ERR_LOG( "asrtr_diag", "Received unexpected message on diag channel" );
                return ASRT_INVALID_EVENT_ERR;
        }
        return ASRT_SUCCESS;
}

enum asrt_status asrtr_diag_client_init(
    struct asrtr_diag_client* diag,
    struct asrt_node*         prev,
    struct asrt_sender        sender )
{
        if ( !diag || !prev ) {
                ASRT_ERR_LOG( "asrtr_diag", "Invalid arguments to diag init" );
                return ASRT_INIT_ERR;
        }
        *diag = ( struct asrtr_diag_client ){
            .node =
                ( struct asrt_node ){
                    .chid     = ASRT_DIAG,
                    .e_cb_ptr = diag,
                    .e_cb     = asrtr_diag_event,
                    .next     = NULL,
                },
            .sendr = sender,
        };
        asrt_node_link( prev, &diag->node );
        return ASRT_SUCCESS;
}

void asrtr_diag_client_deinit( struct asrtr_diag_client* diag )
{
        if ( !diag )
                return;
        asrt_node_unlink( &diag->node );
}

static inline enum asrt_status asrtr_diag_send_cb( void* ptr, struct asrt_rec_span* sp )
{
        struct asrtr_diag_client* diag = (struct asrtr_diag_client*) ptr;
        return asrt_send( &diag->sendr, ASRT_DIAG, sp, NULL, NULL );
}

void asrtr_diag_client_record(
    struct asrtr_diag_client* diag,
    char const*               file,
    uint32_t                  line,
    char const*               extra )
{
        ASRT_ASSERT( diag );
        ASRT_ASSERT( file );

        ASRT_INF_LOG( "asrtr_diag", "Sending diag message: %s:%u", file, line );

        enum asrt_status st =
            asrt_msg_rtoc_diag_record( file, line, extra, asrtr_diag_send_cb, diag );
        if ( st != ASRT_SUCCESS )
                ASRT_ERR_LOG(
                    "asrtr_diag", "Failed to send diag message: %s", asrt_status_to_str( st ) );
}

static inline enum asrt_status asrtr_send( void* r, struct asrt_rec_span* sp )
{
        ASRT_ASSERT( r && sp );
        return asrt_send( &( (struct asrtr_reactor*) r )->sendr, ASRT_CORE, sp, NULL, NULL );
}

static enum asrt_status asrtr_send_test_error(
    struct asrtr_reactor* reac,
    uint16_t              test_id,
    uint32_t              run_id )
{
        enum asrt_status st = asrt_msg_rtoc_test_start( test_id, run_id, asrtr_send, reac );
        if ( st != ASRT_SUCCESS )
                return st;
        return asrt_msg_rtoc_test_result( run_id, ASRT_TEST_ERROR, asrtr_send, reac );
}


static enum asrt_status asrtr_reactor_tick_flag_test_info( struct asrtr_reactor* reac )
{
        struct asrtr_test* t = reac->first_test;
        uint32_t           i = reac->recv_test_info_id;
        while ( i-- > 0 && t )
                t = t->next;
        if ( !t ) {
                if ( asrt_msg_rtoc_test_info(
                         reac->recv_test_info_id,
                         ASRT_TEST_INFO_MISSING_TEST_ERR,
                         "",
                         0,
                         asrtr_send,
                         reac ) != ASRT_SUCCESS ) {
                        ASRT_ERR_LOG( "asrtr_asrtr", "Failed to send missing test info" );
                        return ASRT_SEND_ERR;
                }
        } else if (
            asrt_msg_rtoc_test_info(
                reac->recv_test_info_id,
                ASRT_TEST_INFO_SUCCESS,
                t->desc,
                strlen( t->desc ),
                asrtr_send,
                reac ) != ASRT_SUCCESS ) {
                ASRT_ERR_LOG( "asrtr_asrtr", "Failed to send test info" );
                return ASRT_SEND_ERR;
        }
        return ASRT_SUCCESS;
}

static enum asrt_status asrtr_reactor_tick_flag_test_start( struct asrtr_reactor* reac )
{
        if ( reac->state != ASRTR_REAC_IDLE ) {
                if ( asrtr_send_test_error(
                         reac, reac->recv_test_start_id, reac->recv_test_run_id ) !=
                     ASRT_SUCCESS ) {
                        ASRT_ERR_LOG( "asrtr_asrtr", "Failed to send busy test error result" );
                        return ASRT_SEND_ERR;
                }
                return ASRT_SUCCESS;
        }
        struct asrtr_test* t = reac->first_test;
        uint32_t           i = reac->recv_test_start_id;
        while ( i-- > 0 && t )
                t = t->next;
        if ( !t ) {
                if ( asrtr_send_test_error(
                         reac, reac->recv_test_start_id, reac->recv_test_run_id ) !=
                     ASRT_SUCCESS ) {
                        ASRT_ERR_LOG( "asrtr_asrtr", "Failed to send missing test error result" );
                        return ASRT_SEND_ERR;
                }
        } else {
                reac->test_info = ( struct asrtr_test_input ){
                    .test_ptr   = t->ptr,
                    .continue_f = t->start_f,
                    .run_id     = reac->recv_test_run_id,
                };
                reac->record = ( struct asrtr_record ){
                    .state = ASRTR_TEST_INIT,
                    .inpt  = &reac->test_info,
                };
                reac->state = ASRTR_REAC_TEST_EXEC;
                if ( asrt_msg_rtoc_test_start(
                         reac->recv_test_start_id, reac->record.inpt->run_id, asrtr_send, reac ) !=
                     ASRT_SUCCESS ) {
                        ASRT_ERR_LOG( "asrtr_asrtr", "Failed to send test start" );
                        return ASRT_SEND_ERR;
                }
        }
        return ASRT_SUCCESS;
}

static enum asrt_status asrtr_reactor_tick_flag_desc( struct asrtr_reactor* reac )
{
        if ( asrt_msg_rtoc_desc( reac->desc, strlen( reac->desc ), asrtr_send, reac ) !=
             ASRT_SUCCESS ) {
                ASRT_ERR_LOG( "asrtr_asrtr", "Failed to send description" );
                return ASRT_SEND_ERR;
        }
        return ASRT_SUCCESS;
}

static enum asrt_status asrtr_reactor_tick_flag_proto_ver( struct asrtr_reactor* reac )
{
        if ( asrt_msg_rtoc_proto_version(
                 ASRT_PROTO_MAJOR, ASRT_PROTO_MINOR, ASRT_PROTO_PATCH, asrtr_send, reac ) !=
             ASRT_SUCCESS ) {
                ASRT_ERR_LOG( "asrtr_asrtr", "Failed to send protocol version" );
                return ASRT_SEND_ERR;
        }
        return ASRT_SUCCESS;
}

static enum asrt_status asrtr_reactor_tick_flag_tc( struct asrtr_reactor* reac )
{
        uint16_t           count = 0;
        struct asrtr_test* t     = reac->first_test;
        while ( t != NULL )
                ++count, t = t->next;
        ASRT_INF_LOG( "asrtr_asrtr", "Sending test count: %u", count );
        if ( asrt_msg_rtoc_test_count( count, asrtr_send, reac ) != ASRT_SUCCESS ) {
                ASRT_ERR_LOG( "asrtr_asrtr", "Failed to send test count" );
                return ASRT_SEND_ERR;
        }
        return ASRT_SUCCESS;
}

static enum asrt_status asrtr_reactor_tick_flags( struct asrtr_reactor* reac )
{
        ASRT_ASSERT( reac );
        ASRT_ASSERT( reac->desc );

        uint32_t mask = 0;

        if ( reac->flags & ASRTR_FLAG_DESC ) {
                mask = ~ASRTR_FLAG_DESC;
                ASRT_INF_LOG( "asrtr_asrtr", "Sending description" );
                if ( asrtr_reactor_tick_flag_desc( reac ) != ASRT_SUCCESS )
                        return ASRT_SEND_ERR;
        } else if ( reac->flags & ASRTR_FLAG_PROTO_VER ) {
                mask = ~ASRTR_FLAG_PROTO_VER;
                ASRT_INF_LOG( "asrtr_asrtr", "Sending protocol version" );
                if ( asrtr_reactor_tick_flag_proto_ver( reac ) != ASRT_SUCCESS )
                        return ASRT_SEND_ERR;
        } else if ( reac->flags & ASRTR_FLAG_TC ) {
                mask = ~ASRTR_FLAG_TC;
                if ( asrtr_reactor_tick_flag_tc( reac ) != ASRT_SUCCESS )
                        return ASRT_SEND_ERR;
        } else if ( reac->flags & ASRTR_FLAG_TI ) {
                mask = ~ASRTR_FLAG_TI;
                ASRT_INF_LOG( "asrtr_asrtr", "Sending test %u info", reac->recv_test_info_id );
                if ( asrtr_reactor_tick_flag_test_info( reac ) != ASRT_SUCCESS )
                        return ASRT_SEND_ERR;
        } else if ( reac->flags & ASRTR_FLAG_TSTART ) {
                mask = ~ASRTR_FLAG_TSTART;
                ASRT_INF_LOG(
                    "asrtr_asrtr",
                    "Starting test %u, run ID: %u",
                    reac->recv_test_start_id,
                    reac->recv_test_run_id );
                if ( asrtr_reactor_tick_flag_test_start( reac ) != ASRT_SUCCESS )
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

static enum asrt_status asrtr_reactor_tick_exec( struct asrtr_reactor* reac )
{
        struct asrtr_record* record = &reac->record;
        ASRT_ASSERT( record );
        ASRT_ASSERT( record->inpt->continue_f );

        if ( record->inpt->continue_f( record ) != ASRT_SUCCESS )
                record->state = ASRTR_TEST_ERROR;

        switch ( record->state ) {
        case ASRTR_TEST_INIT:
        case ASRTR_TEST_RUNNING:
                break;
        case ASRTR_TEST_ERROR:
        case ASRTR_TEST_FAIL:
        case ASRTR_TEST_PASS:
                reac->state = ASRTR_REAC_TEST_REPORT;
                break;
        }
        return ASRT_SUCCESS;
}

static enum asrt_status asrtr_reactor_tick_report( struct asrtr_reactor* reac )
{
        struct asrtr_record* record = &reac->record;
        asrt_test_result     res    = record->state == ASRTR_TEST_ERROR ? ASRT_TEST_ERROR :
                                      record->state == ASRTR_TEST_FAIL  ? ASRT_TEST_FAILURE :
                                                                          ASRT_TEST_SUCCESS;
        if ( asrt_msg_rtoc_test_result( record->inpt->run_id, res, asrtr_send, reac ) !=
             ASRT_SUCCESS ) {
                ASRT_ERR_LOG( "asrtr_asrtr", "Failed to send test result" );
                return ASRT_SEND_ERR;
        }
        reac->state = ASRTR_REAC_IDLE;
        return ASRT_SUCCESS;
}

static enum asrt_status asrtr_reactor_tick( struct asrtr_reactor* reac )
{
        ASRT_ASSERT( reac );
        ASRT_ASSERT( reac->desc );

        if ( reac->flags & ~ASRTR_PASSIVE_FLAGS )
                return asrtr_reactor_tick_flags( reac );

        switch ( reac->state ) {
        case ASRTR_REAC_TEST_EXEC:
                return asrtr_reactor_tick_exec( reac );
        case ASRTR_REAC_TEST_REPORT:
                return asrtr_reactor_tick_report( reac );
        case ASRTR_REAC_IDLE:
        default:
                break;
        }

        return ASRT_SUCCESS;
}

static enum asrt_status asrtr_reactor_recv( void* data, struct asrt_span buff )
{
        ASRT_ASSERT( data );
        struct asrtr_reactor* r = (struct asrtr_reactor*) data;
        asrt_message_id       id;

        if ( asrt_span_unfit_for( &buff, sizeof( asrt_message_id ) ) ) {
                ASRT_ERR_LOG( "asrtr_asrtr", "Message too short for ID" );
                return ASRT_RECV_ERR;
        }
        asrt_cut_u16( &buff.b, &id );

        enum asrt_message_id_e eid = (enum asrt_message_id_e) id;
        switch ( eid ) {
        case asrt_msg_PROTO_VERSION:
                ASRT_INF_LOG( "asrtr_asrtr", "Protocol version requested" );
                r->flags |= ASRTR_FLAG_PROTO_VER;
                break;
        case asrt_msg_DESC:
                ASRT_INF_LOG( "asrtr_asrtr", "Description requested" );
                r->flags |= ASRTR_FLAG_DESC;
                break;
        case asrt_msg_TEST_COUNT:
                ASRT_INF_LOG( "asrtr_asrtr", "Test count requested" );
                r->flags |= ASRTR_FLAG_TC;
                break;
        case asrt_msg_TEST_INFO: {
                if ( r->flags & ASRTR_FLAG_TI ) {
                        ASRT_ERR_LOG( "asrtr_asrtr", "TEST_INFO already pending" );
                        return ASRT_RECV_UNEXPECTED_ERR;
                }
                if ( asrt_span_unfit_for( &buff, sizeof( uint16_t ) ) ) {
                        ASRT_ERR_LOG( "asrtr_asrtr", "TEST_INFO message too short" );
                        return ASRT_RECV_ERR;
                }
                asrt_cut_u16( &buff.b, &r->recv_test_info_id );
                ASRT_INF_LOG( "asrtr_asrtr", "Test %i info requested", r->recv_test_info_id );
                r->flags |= ASRTR_FLAG_TI;
                break;
        }
        case asrt_msg_TEST_START: {
                if ( r->flags & ASRTR_FLAG_TSTART ) {
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
                r->flags |= ASRTR_FLAG_TSTART;
                break;
        }
        case asrt_msg_TEST_RESULT:
        default:
                ASRT_ERR_LOG( "asrtr_asrtr", "Unknown message ID: %u", id );
                return ASRT_RECV_UNKNOWN_ID_ERR;
        }
        // If not all bytes are consumed - error
        enum asrt_status res = buff.b == buff.e ? ASRT_SUCCESS : ASRT_RECV_ERR;
        if ( res == ASRT_RECV_ERR )
                ASRT_ERR_LOG( "asrtr_asrtr", "Unused bytes: %zu", (size_t) ( buff.e - buff.b ) );
        else
                r->flags |= ASRTR_FLAG_LOCKED;
        return res;
}

enum asrt_status asrtr_test_init(
    struct asrtr_test*  t,
    char const*         desc,
    void*               ptr,
    asrtr_test_callback start_f )
{
        if ( !t || !desc || !start_f ) {
                ASRT_ERR_LOG( "asrtr_asrtr", "Invalid arguments to test init" );
                return ASRT_INIT_ERR;
        }
        *t = ( struct asrtr_test ){
            .desc    = desc,
            .ptr     = ptr,
            .start_f = start_f,
            .next    = NULL,
        };
        return ASRT_SUCCESS;
}

enum asrt_status asrtr_reactor_add_test( struct asrtr_reactor* reac, struct asrtr_test* test )
{
        ASRT_ASSERT( reac );
        ASRT_ASSERT( test );
        if ( reac->flags & ASRTR_FLAG_LOCKED ) {
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

enum asrt_status asrtr_reactor_event( void* p, enum asrt_event_e e, void* arg )
{
        switch ( e ) {
        case ASRT_EVENT_TICK:
                return asrtr_reactor_tick( (struct asrtr_reactor*) p /*, *(uint32_t*) arg*/ );
        case ASRT_EVENT_RECV:
                return asrtr_reactor_recv( p, *(struct asrt_span*) arg );
        }
        ASRT_ERR_LOG( "asrtr_asrtr", "Received unexpected event on reactor channel" );
        return ASRT_INVALID_EVENT_ERR;
}

enum asrt_status asrtr_reactor_init(
    struct asrtr_reactor* reac,
    struct asrt_sender    sender,
    char const*           desc )
{
        if ( !reac || !desc ) {
                ASRT_ERR_LOG( "asrtr_asrtr", "Invalid arguments to reactor init" );
                return ASRT_INIT_ERR;
        }
        *reac = ( struct asrtr_reactor ){
            .node =
                ( struct asrt_node ){
                    .chid     = ASRT_CORE,
                    .e_cb_ptr = reac,
                    .e_cb     = asrtr_reactor_event,
                    .next     = NULL,
                },
            .sendr              = sender,
            .desc               = desc,
            .first_test         = NULL,
            .last_test          = NULL,
            .state              = ASRTR_REAC_IDLE,
            .flags              = 0,
            .recv_test_info_id  = 0,
            .recv_test_start_id = 0,
        };
        return ASRT_SUCCESS;
}

void asrtr_reactor_deinit( struct asrtr_reactor* reac )
{
        if ( !reac )
                return;
        asrt_node_unlink( &reac->node );
}
