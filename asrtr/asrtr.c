
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
#include "../asrtl/core_proto.h"
#include "../asrtl/ecode.h"
#include "./reactor.h"
#include "status.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

uint32_t asrtr_assert( struct asrtr_record* rec, uint32_t x, uint32_t line )
{
        if ( x != 0 )
                return 1;
        rec->state = ASRTR_TEST_FAIL;
        if ( rec->line == 0 )
                rec->line = line;
        return 0;
}

enum asrtr_status asrtr_reactor_init(
    struct asrtr_reactor* reac,
    struct asrtl_sender   sender,
    char const*           desc )
{
        if ( !reac || !desc )
                return ASRTR_REAC_INIT_ERR;
        *reac = ( struct asrtr_reactor ){
            .node =
                ( struct asrtl_node ){
                    .chid     = ASRTL_CORE,
                    .recv_ptr = reac,
                    .recv_cb  = &asrtr_reactor_recv,
                    .next     = NULL,
                },
            .sendr              = sender,
            .desc               = desc,
            .first_test         = NULL,
            .state              = ASRTR_REAC_IDLE,
            .flags              = 0,
            .recv_test_info_id  = 0,
            .recv_test_start_id = 0,
        };
        return ASRTR_SUCCESS;
}

static enum asrtr_status asrtr_reactor_tick_flag_test_info(
    struct asrtr_reactor* reac,
    struct asrtl_span*    buff )
{
        struct asrtr_test* t = reac->first_test;
        uint32_t           i = reac->recv_test_info_id;
        while ( i-- > 0 && t )
                t = t->next;
        if ( !t ) {
                if ( asrtl_msg_rtoc_error( buff, ASRTL_ASE_MISSING_TEST ) != ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
        } else {
                if ( asrtl_msg_rtoc_test_info(
                         buff, reac->recv_test_info_id, t->desc, strlen( t->desc ) ) !=
                     ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
        }
        return ASRTR_SUCCESS;
}

static enum asrtr_status asrtr_reactor_tick_flag_test_start(
    struct asrtr_reactor* reac,
    struct asrtl_span*    buff )
{
        if ( reac->state != ASRTR_REAC_IDLE ) {
                if ( asrtl_msg_rtoc_error( buff, ASRTL_ASE_TEST_ALREADY_RUNNING ) != ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
                return ASRTR_SUCCESS;
        }
        struct asrtr_test* t = reac->first_test;
        uint32_t           i = reac->recv_test_start_id;
        while ( i-- > 0 && t )
                t = t->next;
        if ( !t ) {
                if ( asrtl_msg_rtoc_error( buff, ASRTL_ASE_MISSING_TEST ) != ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
        } else {
                reac->state_data.record = ( struct asrtr_record ){
                    .state      = ASRTR_TEST_INIT,
                    .test_ptr   = t->ptr,
                    .continue_f = t->start_f,
                    .run_id     = reac->recv_test_run_id,
                    .line       = 0,
                };
                reac->state = ASRTR_REAC_TEST_EXEC;
                if ( asrtl_msg_rtoc_test_start(
                         buff, reac->recv_test_start_id, reac->state_data.record.run_id ) !=
                     ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
        }
        return ASRTR_SUCCESS;
}

static inline enum asrtl_status asrtr_send( struct asrtr_reactor* r, uint8_t* b, uint8_t* e )
{
        assert( r && b && e );
        return asrtl_send( &r->sendr, ASRTL_CORE, ( struct asrtl_span ){ b, e } );
}

static enum asrtr_status asrtr_reactor_tick_flags(
    struct asrtr_reactor* reac,
    struct asrtl_span     buff )
{
        assert( reac );
        assert( reac->desc );

        struct asrtl_span sp   = buff;
        uint32_t          mask = 0;

        if ( reac->flags & ASRTR_FLAG_DESC ) {
                mask = ~ASRTR_FLAG_DESC;
                if ( asrtl_msg_rtoc_desc( &sp, reac->desc, strlen( reac->desc ) ) != ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
        } else if ( reac->flags & ASRTR_FLAG_PROTO_VER ) {
                mask = ~ASRTR_FLAG_PROTO_VER;
                // XXX: find better source of the version
                if ( asrtl_msg_rtoc_proto_version( &sp, 0, 1, 0 ) != ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
        } else if ( reac->flags & ASRTR_FLAG_TC ) {
                mask                     = ~ASRTR_FLAG_TC;
                uint16_t           count = 0;
                struct asrtr_test* t     = reac->first_test;
                while ( t )
                        ++count, t = t->next;

                if ( asrtl_msg_rtoc_test_count( &sp, count ) != ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
        } else if ( reac->flags & ASRTR_FLAG_TI ) {
                mask = ~ASRTR_FLAG_TI;
                if ( asrtr_reactor_tick_flag_test_info( reac, &sp ) != ASRTR_SUCCESS )
                        return ASRTR_SEND_ERR;
        } else if ( reac->flags & ASRTR_FLAG_TSTART ) {
                mask = ~ASRTR_FLAG_TSTART;
                if ( asrtr_reactor_tick_flag_test_start( reac, &sp ) != ASRTR_SUCCESS )
                        return ASRTR_SEND_ERR;
        } else {
                // XXX: report an error?
                // this is internal error....
                reac->flags = 0;
        }
        if ( sp.b != buff.b && asrtr_send( reac, buff.b, sp.b ) != ASRTL_SUCCESS )
                return ASRTR_SEND_ERR;

        if ( mask != 0 )
                reac->flags &= mask;

        return ASRTR_SUCCESS;
}

enum asrtr_status asrtr_reactor_tick( struct asrtr_reactor* reac, struct asrtl_span buff )
{
        assert( reac );
        assert( reac->desc );

        if ( reac->flags != 0x00 )
                return asrtr_reactor_tick_flags( reac, buff );


        switch ( reac->state ) {
        case ASRTR_REAC_TEST_EXEC: {
                struct asrtr_record* record = &reac->state_data.record;
                assert( record );
                assert( record->continue_f );

                if ( record->continue_f( record ) != ASRTR_SUCCESS )
                        record->state = ASRTR_TEST_ERROR;

                switch ( record->state ) {
                case ASRTR_TEST_INIT:
                case ASRTR_TEST_RUNNING:
                        break;
                case ASRTR_TEST_ERROR:
                case ASRTR_TEST_FAIL:
                case ASRTR_TEST_PASS: {
                        reac->state = ASRTR_REAC_TEST_REPORT;
                }
                }

                break;
        }
        case ASRTR_REAC_TEST_REPORT: {
                struct asrtr_record* record = &reac->state_data.record;
                struct asrtl_span    sp     = buff;
                if ( asrtl_msg_rtoc_test_result(
                         &sp,
                         record->run_id,
                         record->state == ASRTR_TEST_ERROR ? ASRTL_TEST_ERROR :
                         record->state == ASRTR_TEST_FAIL  ? ASRTL_TEST_FAILURE :
                                                             ASRTL_TEST_SUCCESS,
                         record->line ) != ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
                if ( asrtr_send( reac, buff.b, sp.b ) != ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
                reac->state = ASRTR_REAC_IDLE;
                break;
        }
        case ASRTR_REAC_IDLE:
        default: {
                break;
        }
        }

        return ASRTR_SUCCESS;
}

enum asrtl_status asrtr_reactor_recv( void* data, struct asrtl_span buff )
{
        assert( data );
        struct asrtr_reactor* r = (struct asrtr_reactor*) data;
        asrtl_message_id      id;

        if ( asrtl_buffer_unfit( &buff, sizeof( asrtl_message_id ) ) )
                return ASRTL_RECV_ERR;
        asrtl_cut_u16( &buff.b, &id );

        enum asrtl_message_id_e eid = (enum asrtl_message_id_e) id;
        switch ( eid ) {
        case ASRTL_MSG_PROTO_VERSION:
                r->flags |= ASRTR_FLAG_PROTO_VER;
                break;
        case ASRTL_MSG_DESC:
                r->flags |= ASRTR_FLAG_DESC;
                break;
        case ASRTL_MSG_TEST_COUNT:
                r->flags |= ASRTR_FLAG_TC;
                break;
        // XXX: what will do fast repeat of this message?
        case ASRTL_MSG_TEST_INFO: {
                if ( asrtl_buffer_unfit( &buff, sizeof( uint16_t ) ) )
                        return ASRTL_RECV_ERR;
                asrtl_cut_u16( &buff.b, &r->recv_test_info_id );
                r->flags |= ASRTR_FLAG_TI;
                break;
        }
        // XXX: what will do fast repeat of this message?
        case ASRTL_MSG_TEST_START: {
                if ( asrtl_buffer_unfit( &buff, sizeof( uint16_t ) + sizeof( uint32_t ) ) )
                        return ASRTL_RECV_ERR;
                asrtl_cut_u16( &buff.b, &r->recv_test_start_id );
                asrtl_cut_u32( &buff.b, &r->recv_test_run_id );
                r->flags |= ASRTR_FLAG_TSTART;
                break;
        }
        case ASRTL_MSG_ERROR:
        case ASRTL_MSG_TEST_RESULT:
        default:
                return ASRTL_RECV_UNKNOWN_ID_ERR;
        }
        return buff.b == buff.e ? ASRTL_SUCCESS : ASRTL_RECV_ERR;
}

enum asrtr_status asrtr_test_init(
    struct asrtr_test*  t,
    char const*         desc,
    void*               ptr,
    asrtr_test_callback start_f )
{
        if ( !t || !desc || !start_f )
                return ASRTR_TEST_INIT_ERR;
        *t = ( struct asrtr_test ){
            .desc    = desc,
            .ptr     = ptr,
            .start_f = start_f,
            .next    = NULL,
        };
        return ASRTR_SUCCESS;
}

void asrtr_reactor_add_test( struct asrtr_reactor* reac, struct asrtr_test* test )
{
        // XXX: disable test registration after ticking starts?
        assert( reac );
        assert( test );
        struct asrtr_test** t = &reac->first_test;
        while ( *t )
                t = &( *t )->next;
        *t = test;
}
