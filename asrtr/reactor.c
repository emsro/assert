
#include "./reactor.h"

#include "../asrtl/core_proto.h"
#include "status.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

enum asrtr_status
asrtr_reactor_init( struct asrtr_reactor* rec, struct asrtl_sender* sender, char const* desc )
{
        if ( !rec || !sender || !desc )
                return ASRTR_REAC_INIT_ERR;
        *rec = ( struct asrtr_reactor ){
            .node =
                ( struct asrtl_node ){
                    .chid      = ASRTL_CORE,
                    .recv_data = rec,
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
asrtr_reactor_tick( struct asrtr_reactor* rec, uint8_t* buffer, uint32_t buffer_size )
{
        assert( rec );
        assert( rec->desc );
        enum asrtr_status res = ASRTR_SUCCESS;

        uint8_t* p    = buffer;
        uint32_t size = buffer_size;

        if ( rec->flags & ASRTR_FLAG_DESC ) {
                rec->flags &= ~ASRTR_FLAG_DESC;
                if ( asrtl_msg_rtoc_desc( &p, &size, rec->desc, strlen( rec->desc ) ) !=
                     ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
        } else if ( rec->flags & ASRTR_FLAG_PROTO_VER ) {
                rec->flags &= ~ASRTR_FLAG_PROTO_VER;
                // XXX: find better source of the version
                if ( asrtl_msg_rtoc_proto_version( &p, &size, 0, 0, 0 ) != ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;

        } else if ( rec->flags & ASRTR_FLAG_TC ) {
                rec->flags &= ~ASRTR_FLAG_TC;
                uint16_t           count = 0;
                struct asrtr_test* t     = rec->first_test;
                while ( t )
                        ++count, t = t->next;

                if ( asrtl_msg_rtoc_count( &p, &size, count ) != ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;
        } else if ( rec->flags & ASRTR_FLAG_TI ) {
                rec->flags &= ~ASRTR_FLAG_TI;
                struct asrtr_test* t = rec->first_test;
                uint32_t           i = rec->test_info_id;
                while ( i-- > 0 && t )
                        t = t->next;
                if ( !t ) {
                        char const* msg = "Failed to find test";
                        if ( asrtl_msg_rtoc_error( &p, &size, msg, strlen( msg ) ) !=
                             ASRTL_SUCCESS )
                                return ASRTR_SEND_ERR;
                } else {
                        if ( asrtl_msg_rtoc_test_info(
                                 &p, &size, rec->test_info_id, t->name, strlen( t->name ) ) !=
                             ASRTL_SUCCESS )
                                return ASRTR_SEND_ERR;
                }
        }
        if ( size != buffer_size )
                if ( asrtl_send( rec->sendr, ASRTL_CORE, buffer, buffer_size - size ) !=
                     ASRTL_SUCCESS )
                        return ASRTR_SEND_ERR;

        switch ( rec->state ) {
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
        case ASRTL_MSG_TEST_INFO: {
                if ( msg_size < sizeof( uint16_t ) )
                        return ASRTL_RECV_ERR;
                asrtl_cut_u16( &msg, &msg_size, &r->test_info_id );
                r->flags |= ASRTR_FLAG_TI;
                break;
        }
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

void asrtr_add_test( struct asrtr_reactor* rec, struct asrtr_test* test )
{
        // XXX: disable test registration after ticking starts?
        assert( rec );
        assert( test );
        struct asrtr_test** t = &rec->first_test;
        while ( *t )
                t = &( *t )->next;
        *t = test;
}
