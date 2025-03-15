
#include "./reactor.h"

#include "../asrtl/core_proto.h"
#include "status.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

enum asrtr_status
asrtr_reactor_init( struct asrtr_reactor* reac, struct asrtl_sender* sender, char const* desc )
{
        if ( !reac || !sender || !desc )
                return ASRTR_REAC_INIT_ERR;
        *reac = ( struct asrtr_reactor ){
            .node =
                ( struct asrtl_node ){
                    .chid      = ASRTL_CORE,
                    .recv_data = reac,
                    .recv_fn   = &asrtr_reactor_recv,
                    .next      = NULL,
                },
            .sendr      = sender,
            .desc       = desc,
            .first_test = NULL,
            .state      = ASRTR_REAC_IDLE,
            .flags      = 0,
        };
        return ASRTR_SUCCESS;
}

static enum asrtr_status
asrtr_reactor_tick_flag_test_info( struct asrtr_reactor* reac, struct asrtl_span* buff )
{
        struct asrtr_test* t = reac->first_test;
        uint32_t           i = reac->recv_test_info_id;
        while ( i-- > 0 && t )
                t = t->next;
        if ( !t ) {
                char const* msg = "Failed to find test";
                if ( asrtl_msg_rtoc_error( buff, msg, strlen( msg ) ) != ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
        } else {
                if ( asrtl_msg_rtoc_test_info(
                         buff, reac->recv_test_info_id, t->name, strlen( t->name ) ) !=
                     ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
        }
        return ASRTR_SUCCESS;
}

static enum asrtr_status
asrtr_reactor_tick_flag_test_start( struct asrtr_reactor* reac, struct asrtl_span* buff )
{
        struct asrtr_test* t = reac->first_test;
        uint32_t           i = reac->recv_test_start_id;
        while ( i-- > 0 && t )
                t = t->next;
        if ( !t ) {
                char const* msg = "Failed to find test";
                if ( asrtl_msg_rtoc_error( buff, msg, strlen( msg ) ) != ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
        } else if ( reac->state != ASRTR_REAC_IDLE ) {
                char const* msg = "Test already running";
                if ( asrtl_msg_rtoc_error( buff, msg, strlen( msg ) ) != ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
        } else {
                reac->state_data.record = ( struct asrtr_record ){
                    .state      = ASRTR_TEST_INIT,
                    .data       = t->data,
                    .continue_f = t->start_f,
                };
                reac->state = ASRTR_REAC_TEST_EXEC;
                if ( asrtl_msg_rtoc_test_start( buff, reac->recv_test_start_id ) != ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
        }
        return ASRTR_SUCCESS;
}

static enum asrtr_status
asrtr_reactor_tick_flags( struct asrtr_reactor* reac, struct asrtl_span buff )
{
        assert( reac );
        assert( reac->desc );

        struct asrtl_span subspan = buff;

        if ( reac->flags & ASRTR_FLAG_DESC ) {
                reac->flags &= ~ASRTR_FLAG_DESC;
                if ( asrtl_msg_rtoc_desc( &subspan, reac->desc, strlen( reac->desc ) ) !=
                     ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
        } else if ( reac->flags & ASRTR_FLAG_PROTO_VER ) {
                reac->flags &= ~ASRTR_FLAG_PROTO_VER;
                // XXX: find better source of the version
                if ( asrtl_msg_rtoc_proto_version( &subspan, 0, 0, 0 ) != ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;

        } else if ( reac->flags & ASRTR_FLAG_TC ) {
                reac->flags &= ~ASRTR_FLAG_TC;
                uint16_t           count = 0;
                struct asrtr_test* t     = reac->first_test;
                while ( t )
                        ++count, t = t->next;

                if ( asrtl_msg_rtoc_count( &subspan, count ) != ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
        } else if ( reac->flags & ASRTR_FLAG_TI ) {
                reac->flags &= ~ASRTR_FLAG_TI;
                if ( asrtr_reactor_tick_flag_test_info( reac, &subspan ) != ASRTR_SUCCESS )
                        return ASRTR_SEND_ERR;
        } else if ( reac->flags & ASRTR_FLAG_TSTART ) {
                reac->flags &= ~ASRTR_FLAG_TSTART;
                if ( asrtr_reactor_tick_flag_test_start( reac, &subspan ) != ASRTR_SUCCESS )
                        return ASRTR_SEND_ERR;
        } else {
                // XXX: report an error?
                // this is internal error....
                reac->flags = 0;
        }

        if ( asrtl_send( reac->sendr, ASRTL_CORE, ( struct asrtl_span ){ buff.b, subspan.b } ) !=
             ASRTL_SUCCESS )
                return ASRTR_SEND_ERR;

        return ASRTR_SUCCESS;
}

enum asrtr_status asrtr_reactor_tick( struct asrtr_reactor* reac, struct asrtl_span buff )
{
        assert( reac );
        assert( reac->desc );

        if ( reac->flags != 0x00 )
                return asrtr_reactor_tick_flags( reac, buff );

        struct asrtl_span subspan = buff;

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
                if ( asrtl_msg_rtoc_test_result(
                         &subspan,
                         record->state == ASRTR_TEST_ERROR ? ASRTL_TEST_ERROR :
                         record->state == ASRTR_TEST_FAIL  ? ASRTL_TEST_FAILURE :
                                                             ASRTL_TEST_SUCCESS ) != ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
                reac->state = ASRTR_REAC_IDLE;
                break;
        }
        case ASRTR_REAC_IDLE:
        default: {
                break;
        }
        }

        if ( subspan.b != buff.b ) {
                if ( asrtl_send(
                         reac->sendr, ASRTL_CORE, ( struct asrtl_span ){ buff.b, subspan.b } ) !=
                     ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
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
                if ( asrtl_buffer_unfit( &buff, sizeof( uint16_t ) ) )
                        return ASRTL_RECV_ERR;
                asrtl_cut_u16( &buff.b, &r->recv_test_start_id );
                r->flags |= ASRTR_FLAG_TSTART;
                break;
        }
        case ASRTL_MSG_ERROR:
        default:
                return ASRTL_UNKNOWN_ID_ERR;
        }
        return buff.b == buff.e ? ASRTL_SUCCESS : ASRTL_RECV_ERR;
}

enum asrtr_status
asrtr_test_init( struct asrtr_test* t, char const* name, void* data, asrtr_test_callback start_f )
{
        if ( !t || !name || !start_f )
                return ASRTR_TEST_INIT_ERR;
        *t = ( struct asrtr_test ){
            .name    = name,
            .data    = data,
            .start_f = start_f,
            .next    = NULL,
        };
        return ASRTR_SUCCESS;
}

void asrtr_add_test( struct asrtr_reactor* reac, struct asrtr_test* test )
{
        // XXX: disable test registration after ticking starts?
        assert( reac );
        assert( test );
        struct asrtr_test** t = &reac->first_test;
        while ( *t )
                t = &( *t )->next;
        *t = test;
}
