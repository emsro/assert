#include "../asrtr/reactor.h"

#include <stdlib.h>
#include <string.h>
#include <unity.h>

void setUp( void )
{
}
void tearDown( void )
{
}

struct data_ll
{
        uint8_t*        data;
        uint32_t        data_size;
        asrtl_chann_id  id;
        struct data_ll* next;
};

void free_data_ll( struct data_ll* p )
{
        if ( p->next )
                free_data_ll( p->next );
        free( p->data );
        free( p );
}

enum asrtl_status
sender_collect( void* data, asrtl_chann_id id, uint8_t const* msg, uint32_t msg_size )
{
        assert( data );
        assert( msg );
        struct data_ll** lnode = (struct data_ll**) data;
        struct data_ll*  p     = malloc( sizeof( struct data_ll ) );
        p->data                = malloc( msg_size );
        memcpy( p->data, msg, msg_size );
        p->data_size = msg_size;
        p->id        = id;
        p->next      = *lnode;
        *lnode       = p;
        return ASRTL_SUCCESS;
}

void setup_sender_collector( struct asrtl_sender* s, struct data_ll** data )
{
        *s = ( struct asrtl_sender ){
            .send_data = (void*) data,
            .send_fn   = &sender_collect,
        };
}

enum asrtr_status dataless_test_fun( struct asrtr_record* x )
{
        (void) x;
        return ASRTR_SUCCESS;
}

void test_reactor_init( void )
{
        struct asrtr_reactor rec;
        struct data_ll*      collected = NULL;
        struct asrtl_sender  send;
        setup_sender_collector( &send, &collected );

        asrtr_reactor_init( &rec, &send );
        TEST_ASSERT_NULL( rec.first_test );
        TEST_ASSERT_EQUAL( rec.node.chid, ASRTL_CORE );
        TEST_ASSERT_EQUAL( rec.state, ASRTR_REC_IDLE );

        enum asrtr_status st;
        struct asrtr_test t1, t2;
        st = asrtr_test_init( &t1, NULL, NULL, &dataless_test_fun );
        TEST_ASSERT_EQUAL( st, ASRTR_TEST_INIT_ERR );
        st = asrtr_test_init( &t1, "test1", NULL, NULL );
        TEST_ASSERT_EQUAL( st, ASRTR_TEST_INIT_ERR );

        st = asrtr_test_init( &t1, "test1", NULL, &dataless_test_fun );
        TEST_ASSERT_EQUAL( st, ASRTR_SUCCESS );
        st = asrtr_test_init( &t2, "test2", NULL, &dataless_test_fun );
        TEST_ASSERT_EQUAL( st, ASRTR_SUCCESS );

        asrtr_add_test( &rec, &t1 );
        asrtr_add_test( &rec, &t2 );

        TEST_ASSERT_NULL( t2.next );
        TEST_ASSERT_EQUAL_PTR( t1.next, &t2 );
        TEST_ASSERT_EQUAL_PTR( rec.first_test, &t1 );

        st = asrtr_reactor_list_event( &rec );
        TEST_ASSERT_EQUAL( st, ASRTR_SUCCESS );
        TEST_ASSERT_EQUAL( rec.state, ASRTR_REC_LIST );
        st = asrtr_reactor_list_event( &rec );
        TEST_ASSERT_EQUAL( st, ASRTR_BUSY_ERR );
        TEST_ASSERT_EQUAL( rec.state, ASRTR_REC_LIST );

        TEST_ASSERT_NULL( collected );
}

void test_reactor_list( void )
{
        enum asrtr_status    st;
        struct asrtr_reactor rec;
        struct data_ll*      collected = NULL;
        struct asrtl_sender  send;
        setup_sender_collector( &send, &collected );
        asrtr_reactor_init( &rec, &send );

        st = asrtr_reactor_list_event( &rec );
        TEST_ASSERT_EQUAL( st, ASRTR_SUCCESS );
        TEST_ASSERT_EQUAL( ASRTR_REC_LIST, rec.state );

        st = asrtr_reactor_list_event( &rec );
        TEST_ASSERT_EQUAL( st, ASRTR_BUSY_ERR );

        st = asrtr_reactor_tick( &rec );
        TEST_ASSERT_EQUAL( st, ASRTR_SUCCESS );
        TEST_ASSERT_EQUAL( ASRTR_REC_IDLE, rec.state );

        struct asrtr_test t1;
        st = asrtr_test_init( &t1, "test1", NULL, &dataless_test_fun );
        asrtr_add_test( &rec, &t1 );

        st = asrtr_reactor_list_event( &rec );
        TEST_ASSERT_EQUAL( st, ASRTR_SUCCESS );

        st = asrtr_reactor_tick( &rec );
        TEST_ASSERT_EQUAL( st, ASRTR_SUCCESS );
        TEST_ASSERT_EQUAL( ASRTR_REC_IDLE, rec.state );

        TEST_ASSERT_NOT_NULL( collected );
        free_data_ll( collected );
}

int main( void )
{
        UNITY_BEGIN();
        RUN_TEST( test_reactor_init );
        RUN_TEST( test_reactor_list );
        return UNITY_END();
}
