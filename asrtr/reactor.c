
#include "./reactor.h"

#include "../asrtl/proto.h"

#include <assert.h>
#include <stddef.h>

void asrtr_rec_init( struct asrtr_reactor* rec )
{
        assert( rec );
        *rec = ( struct asrtr_reactor ){
            .first_test = NULL,
            .state      = ASRTR_REC_IDLE,
        };
}

enum asrtr_status asrtr_rec_list_event( struct asrtr_reactor* rec )
{
        assert( rec );
        if ( rec->state != ASRTR_REC_IDLE )
                return ASRTR_REACTOR_BUSY_ERR;

        rec->state_data.list_next = rec->first_test;
        rec->state                = ASRTR_REC_LIST;

        return ASRTR_SUCCESS;
}
static enum asrtr_status
send_test_info( struct asrtr_reactor_comms* comms, struct asrtr_test* test )
{
        assert( comms );
        assert( test );

        return ASRTR_SUCCESS;
}
enum asrtr_status asrtr_rec_tick( struct asrtr_reactor* rec )
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
                        res = send_test_info( &rec->comms, *p );
                        *p  = ( *p )->next;
                }
                if ( !*p )
                        rec->state = ASRTR_REC_IDLE;
                break;
        }
        }
        return res;
}

enum asrtr_status
asrtr_test_init( struct asrtr_test* t, char const* name, void* data, asrtr_test_callback start_f )
{
        assert( t );
        if ( !name || !start_f )
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
