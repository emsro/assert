
#include "./reactor.h"

#include "../asrtl/core_proto.h"
#include "status.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

enum asrtr_status asrtr_reactor_init( struct asrtr_reactor* rec, struct asrtl_sender* sender )
{
        if ( !rec || !sender )
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
            .first_test = NULL,
            .state      = ASRTR_REC_IDLE,
        };
        return ASRTR_SUCCESS;
}

enum asrtr_status asrtr_reactor_list_event( struct asrtr_reactor* rec )
{
        assert( rec );
        if ( rec->state != ASRTR_REC_IDLE )
                return ASRTR_BUSY_ERR;

        rec->state_data.list_next = rec->first_test;
        rec->state                = ASRTR_REC_LIST;

        return ASRTR_SUCCESS;
}
static enum asrtr_status
asrtr_send_test_info( struct asrtl_sender* sen, asrtl_chann_id chid, struct asrtr_test* test )
{
        assert( sen );
        assert( test );

        uint8_t  data[42];
        uint8_t* p          = data;
        uint32_t free_space = sizeof data;
        if ( asrtl_add_message_id( &p, &free_space, ASRTL_MSG_LIST ) != ASRTL_SUCCESS )
                return ASRTR_SEND_ERR;

        asrtl_fill_buffer( (uint8_t const*) test->name, strlen( test->name ), &p, &free_space );

        enum asrtl_status r = asrtl_send( sen, chid, p, sizeof data - free_space );
        return r == ASRTL_SUCCESS ? ASRTR_SUCCESS : ASRTR_SEND_ERR;
}
enum asrtr_status asrtr_reactor_tick( struct asrtr_reactor* rec )
{
        assert( rec );
        enum asrtr_status res = ASRTR_SUCCESS;
        switch ( rec->state ) {
        case ASRTR_REC_IDLE:
        default: {
                break;
        }
        case ASRTR_REC_LIST: {
                struct asrtr_test** p = &rec->state_data.list_next;
                if ( *p ) {
                        res = asrtr_send_test_info( rec->sendr, rec->node.chid, *p );
                        *p  = ( *p )->next;
                }
                if ( !*p )
                        rec->state = ASRTR_REC_IDLE;
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
        enum asrtr_status     rst;
        asrtl_message_id      id;

        if ( asrtl_cut_message_id( &msg, &msg_size, &id ) != ASRTL_SUCCESS )
                return ASRTL_RECV_ERR;

        switch ( (enum asrtl_message_id_e) id ) {
        case ASRTL_MSG_LIST:
                rst = asrtr_reactor_list_event( r );
                if ( rst == ASRTR_BUSY_ERR )
                        return ASRTL_BUSY_ERR;
                if ( rst != ASRTR_SUCCESS )
                        return ASRTL_RECV_ERR;
                break;
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
