
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
#include "../asrtl/asrtl_assert.h"
#include "../asrtl/core_proto.h"
#include "../asrtl/diag_proto.h"
#include "../asrtl/ecode.h"
#include "../asrtl/log.h"
#include "../asrtl/proto_version.h"
#include "../asrtl/status_to_str.h"
#include "./diag.h"
#include "./reactor.h"
#include "status.h"

#include <stddef.h>
#include <string.h>

void asrtr_fail( struct asrtr_record* rec )
{
        rec->state = ASRTR_TEST_FAIL;
}


static enum asrtl_status asrtr_diag_event( void* p, enum asrtl_event_e e, void* arg )
{
        (void) arg;
        (void) p;
        switch ( e ) {
        case ASRTL_EVENT_TICK:
                return ASRTL_SUCCESS;
        case ASRTL_EVENT_RECV:
                ASRTL_ERR_LOG( "asrtr_diag", "Received unexpected message on diag channel" );
                return ASRTL_INVALID_EVENT_ERR;
        }
        return ASRTL_SUCCESS;
}

enum asrtr_status asrtr_diag_init(
    struct asrtr_diag*  diag,
    struct asrtl_node*  prev,
    struct asrtl_sender sender )
{
        if ( !diag || !prev ) {
                ASRTL_ERR_LOG( "asrtr_diag", "Invalid arguments to diag init" );
                return ASRTR_INIT_ERR;
        }
        *diag = ( struct asrtr_diag ){
            .node =
                ( struct asrtl_node ){
                    .chid     = ASRTL_DIAG,
                    .e_cb_ptr = diag,
                    .e_cb     = asrtr_diag_event,
                    .next     = NULL,
                },
            .sendr = sender,
        };
        prev->next = &diag->node;
        return ASRTR_SUCCESS;
}

static inline enum asrtl_status asrtr_diag_send_cb( void* ptr, struct asrtl_rec_span* sp )
{
        struct asrtr_diag* diag = (struct asrtr_diag*) ptr;
        return asrtl_send( &diag->sendr, ASRTL_DIAG, sp, NULL, NULL );
}

void asrtr_diag_record(
    struct asrtr_diag* diag,
    char const*        file,
    uint32_t           line,
    char const*        extra )
{
        ASRTL_ASSERT( diag );
        ASRTL_ASSERT( file );

        ASRTL_INF_LOG( "asrtr_diag", "Sending diag message: %s:%u", file, line );

        enum asrtl_status st =
            asrtl_msg_rtoc_diag_record( file, line, extra, asrtr_diag_send_cb, diag );
        if ( st != ASRTL_SUCCESS )
                ASRTL_ERR_LOG(
                    "asrtr_diag", "Failed to send diag message: %s", asrtl_status_to_str( st ) );
}

static inline enum asrtl_status asrtr_send( void* r, struct asrtl_rec_span* sp )
{
        ASRTL_ASSERT( r && sp );
        return asrtl_send( &( (struct asrtr_reactor*) r )->sendr, ASRTL_CORE, sp, NULL, NULL );
}


static enum asrtr_status asrtr_reactor_tick_flag_test_info( struct asrtr_reactor* reac )
{
        struct asrtr_test* t = reac->first_test;
        uint32_t           i = reac->recv_test_info_id;
        while ( i-- > 0 && t )
                t = t->next;
        if ( !t ) {
                if ( asrtl_msg_rtoc_error( ASRTL_ASE_MISSING_TEST, asrtr_send, reac ) !=
                     ASRTL_SUCCESS ) {
                        ASRTL_ERR_LOG( "asrtr_asrtr", "Failed to send MISSING_TEST error" );
                        return ASRTR_SEND_ERR;
                }
        } else if (
            asrtl_msg_rtoc_test_info(
                reac->recv_test_info_id, t->desc, strlen( t->desc ), asrtr_send, reac ) !=
            ASRTL_SUCCESS ) {
                ASRTL_ERR_LOG( "asrtr_asrtr", "Failed to send test info" );
                return ASRTR_SEND_ERR;
        }
        return ASRTR_SUCCESS;
}

static enum asrtr_status asrtr_reactor_tick_flag_test_start( struct asrtr_reactor* reac )
{
        if ( reac->state != ASRTR_REAC_IDLE ) {
                if ( asrtl_msg_rtoc_error( ASRTL_ASE_TEST_ALREADY_RUNNING, asrtr_send, reac ) !=
                     ASRTL_SUCCESS ) {
                        ASRTL_ERR_LOG( "asrtr_asrtr", "Failed to send TEST_ALREADY_RUNNING error" );
                        return ASRTR_SEND_ERR;
                }
                return ASRTR_SUCCESS;
        }
        struct asrtr_test* t = reac->first_test;
        uint32_t           i = reac->recv_test_start_id;
        while ( i-- > 0 && t )
                t = t->next;
        if ( !t ) {
                if ( asrtl_msg_rtoc_error( ASRTL_ASE_MISSING_TEST, asrtr_send, reac ) !=
                     ASRTL_SUCCESS ) {
                        ASRTL_ERR_LOG( "asrtr_asrtr", "Failed to send MISSING_TEST error" );
                        return ASRTR_SEND_ERR;
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
                if ( asrtl_msg_rtoc_test_start(
                         reac->recv_test_start_id, reac->record.inpt->run_id, asrtr_send, reac ) !=
                     ASRTL_SUCCESS ) {
                        ASRTL_ERR_LOG( "asrtr_asrtr", "Failed to send test start" );
                        return ASRTR_SEND_ERR;
                }
        }
        return ASRTR_SUCCESS;
}

static enum asrtr_status asrtr_reactor_tick_flag_desc( struct asrtr_reactor* reac )
{
        if ( asrtl_msg_rtoc_desc( reac->desc, strlen( reac->desc ), asrtr_send, reac ) !=
             ASRTL_SUCCESS ) {
                ASRTL_ERR_LOG( "asrtr_asrtr", "Failed to send description" );
                return ASRTR_SEND_ERR;
        }
        return ASRTR_SUCCESS;
}

static enum asrtr_status asrtr_reactor_tick_flag_proto_ver( struct asrtr_reactor* reac )
{
        if ( asrtl_msg_rtoc_proto_version(
                 ASRTL_PROTO_MAJOR, ASRTL_PROTO_MINOR, ASRTL_PROTO_PATCH, asrtr_send, reac ) !=
             ASRTL_SUCCESS ) {
                ASRTL_ERR_LOG( "asrtr_asrtr", "Failed to send protocol version" );
                return ASRTR_SEND_ERR;
        }
        return ASRTR_SUCCESS;
}

static enum asrtr_status asrtr_reactor_tick_flag_tc( struct asrtr_reactor* reac )
{
        uint16_t           count = 0;
        struct asrtr_test* t     = reac->first_test;
        while ( t != NULL )
                ++count, t = t->next;
        ASRTL_INF_LOG( "asrtr_asrtr", "Sending test count: %u", count );
        if ( asrtl_msg_rtoc_test_count( count, asrtr_send, reac ) != ASRTL_SUCCESS ) {
                ASRTL_ERR_LOG( "asrtr_asrtr", "Failed to send test count" );
                return ASRTR_SEND_ERR;
        }
        return ASRTR_SUCCESS;
}

static enum asrtl_status asrtr_reactor_tick_flags( struct asrtr_reactor* reac )
{
        ASRTL_ASSERT( reac );
        ASRTL_ASSERT( reac->desc );

        uint32_t mask = 0;

        if ( reac->flags & ASRTR_FLAG_DESC ) {
                mask = ~ASRTR_FLAG_DESC;
                ASRTL_INF_LOG( "asrtr_asrtr", "Sending description" );
                if ( asrtr_reactor_tick_flag_desc( reac ) != ASRTR_SUCCESS )
                        return ASRTL_SEND_ERR;
        } else if ( reac->flags & ASRTR_FLAG_PROTO_VER ) {
                mask = ~ASRTR_FLAG_PROTO_VER;
                ASRTL_INF_LOG( "asrtr_asrtr", "Sending protocol version" );
                if ( asrtr_reactor_tick_flag_proto_ver( reac ) != ASRTR_SUCCESS )
                        return ASRTL_SEND_ERR;
        } else if ( reac->flags & ASRTR_FLAG_TC ) {
                mask = ~ASRTR_FLAG_TC;
                if ( asrtr_reactor_tick_flag_tc( reac ) != ASRTR_SUCCESS )
                        return ASRTL_SEND_ERR;
        } else if ( reac->flags & ASRTR_FLAG_TI ) {
                mask = ~ASRTR_FLAG_TI;
                ASRTL_INF_LOG( "asrtr_asrtr", "Sending test %u info", reac->recv_test_info_id );
                if ( asrtr_reactor_tick_flag_test_info( reac ) != ASRTR_SUCCESS )
                        return ASRTL_SEND_ERR;
        } else if ( reac->flags & ASRTR_FLAG_TSTART ) {
                mask = ~ASRTR_FLAG_TSTART;
                ASRTL_INF_LOG(
                    "asrtr_asrtr",
                    "Starting test %u, run ID: %u",
                    reac->recv_test_start_id,
                    reac->recv_test_run_id );
                if ( asrtr_reactor_tick_flag_test_start( reac ) != ASRTR_SUCCESS )
                        return ASRTL_SEND_ERR;
        } else {
                ASRTL_ERR_LOG( "asrtr_asrtr", "Unknown flag bits set: 0x%x", reac->flags );
                reac->flags = 0;
                return ASRTL_INTERNAL_ERR;
        }

        if ( mask != 0 )
                reac->flags &= mask;

        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtr_reactor_tick_exec( struct asrtr_reactor* reac )
{
        struct asrtr_record* record = &reac->record;
        ASRTL_ASSERT( record );
        ASRTL_ASSERT( record->inpt->continue_f );

        if ( record->inpt->continue_f( record ) != ASRTR_SUCCESS )
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
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtr_reactor_tick_report( struct asrtr_reactor* reac )
{
        struct asrtr_record* record = &reac->record;
        asrtl_test_result    res    = record->state == ASRTR_TEST_ERROR ? ASRTL_TEST_ERROR :
                                      record->state == ASRTR_TEST_FAIL  ? ASRTL_TEST_FAILURE :
                                                                          ASRTL_TEST_SUCCESS;
        if ( asrtl_msg_rtoc_test_result( record->inpt->run_id, res, asrtr_send, reac ) !=
             ASRTL_SUCCESS ) {
                ASRTL_ERR_LOG( "asrtr_asrtr", "Failed to send test result" );
                return ASRTL_SEND_ERR;
        }
        reac->state = ASRTR_REAC_IDLE;
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtr_reactor_tick( struct asrtr_reactor* reac )
{
        ASRTL_ASSERT( reac );
        ASRTL_ASSERT( reac->desc );

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

        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtr_reactor_recv( void* data, struct asrtl_span buff )
{
        ASRTL_ASSERT( data );
        struct asrtr_reactor* r = (struct asrtr_reactor*) data;
        asrtl_message_id      id;

        if ( asrtl_span_unfit_for( &buff, sizeof( asrtl_message_id ) ) ) {
                ASRTL_ERR_LOG( "asrtr_asrtr", "Message too short for ID" );
                return ASRTL_RECV_ERR;
        }
        asrtl_cut_u16( &buff.b, &id );

        enum asrtl_message_id_e eid = (enum asrtl_message_id_e) id;
        switch ( eid ) {
        case ASRTL_MSG_PROTO_VERSION:
                ASRTL_INF_LOG( "asrtr_asrtr", "Protocol version requested" );
                r->flags |= ASRTR_FLAG_PROTO_VER;
                break;
        case ASRTL_MSG_DESC:
                ASRTL_INF_LOG( "asrtr_asrtr", "Description requested" );
                r->flags |= ASRTR_FLAG_DESC;
                break;
        case ASRTL_MSG_TEST_COUNT:
                ASRTL_INF_LOG( "asrtr_asrtr", "Test count requested" );
                r->flags |= ASRTR_FLAG_TC;
                break;
        case ASRTL_MSG_TEST_INFO: {
                if ( r->flags & ASRTR_FLAG_TI ) {
                        ASRTL_ERR_LOG( "asrtr_asrtr", "TEST_INFO already pending" );
                        return ASRTL_RECV_UNEXPECTED_ERR;
                }
                if ( asrtl_span_unfit_for( &buff, sizeof( uint16_t ) ) ) {
                        ASRTL_ERR_LOG( "asrtr_asrtr", "TEST_INFO message too short" );
                        return ASRTL_RECV_ERR;
                }
                asrtl_cut_u16( &buff.b, &r->recv_test_info_id );
                ASRTL_INF_LOG( "asrtr_asrtr", "Test %i info requested", r->recv_test_info_id );
                r->flags |= ASRTR_FLAG_TI;
                break;
        }
        case ASRTL_MSG_TEST_START: {
                if ( r->flags & ASRTR_FLAG_TSTART ) {
                        ASRTL_ERR_LOG( "asrtr_asrtr", "TEST_START already pending" );
                        return ASRTL_RECV_UNEXPECTED_ERR;
                }
                if ( asrtl_span_unfit_for( &buff, sizeof( uint16_t ) + sizeof( uint32_t ) ) ) {
                        ASRTL_ERR_LOG( "asrtr_asrtr", "TEST_START message too short" );
                        return ASRTL_RECV_ERR;
                }
                asrtl_cut_u16( &buff.b, &r->recv_test_start_id );
                asrtl_cut_u32( &buff.b, &r->recv_test_run_id );
                ASRTL_INF_LOG(
                    "asrtr_asrtr",
                    "Test %i start requested, run ID: %u",
                    r->recv_test_start_id,
                    r->recv_test_run_id );
                r->flags |= ASRTR_FLAG_TSTART;
                break;
        }
        case ASRTL_MSG_ERROR:
        case ASRTL_MSG_TEST_RESULT:
        default:
                ASRTL_ERR_LOG( "asrtr_asrtr", "Unknown message ID: %u", id );
                return ASRTL_RECV_UNKNOWN_ID_ERR;
        }
        // If not all bytes are consumed - error
        enum asrtl_status res = buff.b == buff.e ? ASRTL_SUCCESS : ASRTL_RECV_ERR;
        if ( res == ASRTL_RECV_ERR )
                ASRTL_ERR_LOG( "asrtr_asrtr", "Unused bytes: %zu", (size_t) ( buff.e - buff.b ) );
        else
                r->flags |= ASRTR_FLAG_LOCKED;
        return res;
}

enum asrtr_status asrtr_test_init(
    struct asrtr_test*  t,
    char const*         desc,
    void*               ptr,
    asrtr_test_callback start_f )
{
        if ( !t || !desc || !start_f ) {
                ASRTL_ERR_LOG( "asrtr_asrtr", "Invalid arguments to test init" );
                return ASRTR_TEST_INIT_ERR;
        }
        *t = ( struct asrtr_test ){
            .desc    = desc,
            .ptr     = ptr,
            .start_f = start_f,
            .next    = NULL,
        };
        return ASRTR_SUCCESS;
}

enum asrtr_status asrtr_reactor_add_test( struct asrtr_reactor* reac, struct asrtr_test* test )
{
        ASRTL_ASSERT( reac );
        ASRTL_ASSERT( test );
        if ( reac->flags & ASRTR_FLAG_LOCKED ) {
                ASRTL_ERR_LOG( "asrtr_asrtr", "Test registration locked after first recv" );
                return ASRTR_TEST_REG_ERR;
        }
        test->next = NULL;
        if ( reac->last_test )
                reac->last_test->next = test;
        else
                reac->first_test = test;
        reac->last_test = test;
        return ASRTR_SUCCESS;
}

enum asrtl_status asrtr_reactor_event( void* p, enum asrtl_event_e e, void* arg )
{
        switch ( e ) {
        case ASRTL_EVENT_TICK:
                return asrtr_reactor_tick( (struct asrtr_reactor*) p /*, *(uint32_t*) arg*/ );
        case ASRTL_EVENT_RECV:
                return asrtr_reactor_recv( p, *(struct asrtl_span*) arg );
        }
        ASRTL_ERR_LOG( "asrtr_asrtr", "Received unexpected event on reactor channel" );
        return ASRTL_INVALID_EVENT_ERR;
}

enum asrtr_status asrtr_reactor_init(
    struct asrtr_reactor* reac,
    struct asrtl_sender   sender,
    char const*           desc )
{
        if ( !reac || !desc ) {
                ASRTL_ERR_LOG( "asrtr_asrtr", "Invalid arguments to reactor init" );
                return ASRTR_INIT_ERR;
        }
        *reac = ( struct asrtr_reactor ){
            .node =
                ( struct asrtl_node ){
                    .chid     = ASRTL_CORE,
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
        return ASRTR_SUCCESS;
}
