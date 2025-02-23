
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
            .state      = ASRTR_REC_IDLE,
            .flags      = 0,
        };
        return ASRTR_SUCCESS;
}

enum asrtr_status
asrtr_reactor_tick( struct asrtr_reactor* reac, uint8_t* buffer, uint32_t buffer_size )
{
        assert( reac );
        assert( reac->desc );
        enum asrtr_status res = ASRTR_SUCCESS;

        uint8_t* p    = buffer;
        uint32_t size = buffer_size;

        if ( reac->flags & ASRTR_FLAG_DESC ) {
                reac->flags &= ~ASRTR_FLAG_DESC;
                if ( asrtl_msg_rtoc_desc( &p, &size, reac->desc, strlen( reac->desc ) ) !=
                     ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
        } else if ( reac->flags & ASRTR_FLAG_PROTO_VER ) {
                reac->flags &= ~ASRTR_FLAG_PROTO_VER;
                // XXX: find better source of the version
                if ( asrtl_msg_rtoc_proto_version( &p, &size, 0, 0, 0 ) != ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;

        } else if ( reac->flags & ASRTR_FLAG_TC ) {
                reac->flags &= ~ASRTR_FLAG_TC;
                uint16_t           count = 0;
                struct asrtr_test* t     = reac->first_test;
                while ( t )
                        ++count, t = t->next;

                if ( asrtl_msg_rtoc_count( &p, &size, count ) != ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
        } else if ( reac->flags & ASRTR_FLAG_TI ) {
                reac->flags &= ~ASRTR_FLAG_TI;
                struct asrtr_test* t = reac->first_test;
                uint32_t           i = reac->recv_test_info_id;
                while ( i-- > 0 && t )
                        t = t->next;
                if ( !t ) {
                        char const* msg = "Failed to find test";
                        if ( asrtl_msg_rtoc_error( &p, &size, msg, strlen( msg ) ) !=
                             ASRTL_SUCCESS )
                                return ASRTR_SEND_ERR;
                } else {
                        if ( asrtl_msg_rtoc_test_info(
                                 &p, &size, reac->recv_test_info_id, t->name, strlen( t->name ) ) !=
                             ASRTL_SUCCESS )
                                return ASRTR_SEND_ERR;
                }
        } else if ( reac->flags & ASRTR_FLAG_TSTART ) {
                reac->flags &= ~ASRTR_FLAG_TSTART;
                struct asrtr_test* t = reac->first_test;
                uint32_t           i = reac->recv_test_start_id;
                while ( i-- > 0 && t )
                        t = t->next;
                if ( !t ) {
                        char const* msg = "Failed to find test";
                        if ( asrtl_msg_rtoc_error( &p, &size, msg, strlen( msg ) ) !=
                             ASRTL_SUCCESS )
                                return ASRTR_SEND_ERR;
                } else if ( reac->state != ASRTR_REC_IDLE ) {
                        char const* msg = "Test already running";
                        if ( asrtl_msg_rtoc_error( &p, &size, msg, strlen( msg ) ) !=
                             ASRTL_SUCCESS )
                                return ASRTR_SEND_ERR;
                } else {
                        reac->state_data.record = ( struct asrtr_record ){
                            .state      = ASRTR_TEST_INIT,
                            .data       = t->data,
                            .continue_f = t->start_f,
                        };
                        reac->state = ASRTR_REC_TEST_EXEC;
                        if ( asrtl_msg_rtoc_test_start( &p, &size, reac->recv_test_start_id ) !=
                             ASRTL_SUCCESS )
                                return ASRTR_SEND_ERR;
                }
        }
        if ( size != buffer_size ) {
                if ( asrtl_send( reac->sendr, ASRTL_CORE, buffer, buffer_size - size ) !=
                     ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
                return res;
        }

        switch ( reac->state ) {
        case ASRTR_REC_TEST_EXEC: {
                struct asrtr_record* record = &reac->state_data.record;
                assert( record->data );
                assert( record->continue_f );

                if ( record->continue_f( record->data ) != ASRTR_SUCCESS )
                        record->state = ASRTR_TEST_ERROR;

                switch ( record->state ) {
                case ASRTR_TEST_INIT:
                case ASRTR_TEST_RUNNING:
                        break;
                case ASRTR_TEST_ERROR:
                case ASRTR_TEST_FAIL:
                case ASRTR_TEST_PASS: {
                        reac->state = ASRTR_REC_TEST_REPORT;
                }
                }

                break;
        }
        case ASRTR_REC_TEST_REPORT: {
                struct asrtr_record* record = &reac->state_data.record;
                if ( asrtl_msg_rtoc_test_result(
                         &p,
                         &size,
                         record->state == ASRTR_TEST_ERROR ? ASRTL_TEST_ERROR :
                         record->state == ASRTR_TEST_FAIL  ? ASRTL_TEST_FAILURE :
                                                             ASRTL_TEST_SUCCESS ) != ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
                reac->state = ASRTR_REC_IDLE;
                break;
        }
        case ASRTR_REC_IDLE:
        default: {
                break;
        }
        }
        return res;
}
enum asrtl_status asrtr_reactor_recv( void* data, uint8_t const* msg, uint32_t msg_size )
{
        assert( data );
        assert( msg );
        struct asrtr_reactor* r = (struct asrtr_reactor*) data;
        asrtl_message_id      id;

        if ( msg_size < sizeof( asrtl_message_id ) )
                return ASRTL_RECV_ERR;
        asrtl_cut_u16( &msg, &msg_size, &id );

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
                if ( msg_size < sizeof( uint16_t ) )
                        return ASRTL_RECV_ERR;
                asrtl_cut_u16( &msg, &msg_size, &r->recv_test_info_id );
                r->flags |= ASRTR_FLAG_TI;
                break;
        }
        // XXX: what will do fast repeat of this message?
        case ASRTL_MSG_TEST_START: {
                if ( msg_size < sizeof( uint16_t ) )
                        return ASRTL_RECV_ERR;
                asrtl_cut_u16( &msg, &msg_size, &r->recv_test_start_id );
                r->flags |= ASRTR_FLAG_TSTART;
                break;
        }
        case ASRTL_MSG_ERROR:
        default:
                return ASRTL_UNKNOWN_ID_ERR;
        }
        return msg_size == 0 ? ASRTL_SUCCESS : ASRTL_RECV_ERR;
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
