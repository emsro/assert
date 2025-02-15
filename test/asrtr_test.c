#include "../asrtr/reactor.h"

#include <unity.h>

void setUp( void )
{
}
void tearDown( void )
{
}

enum asrtr_status dataless_test_fun( struct asrtr_record* x )
{
        (void) x;
        return ASRTR_SUCCESS;
}

void test_cut_chann_id( void )
{
        struct asrtr_reactor rec;
        asrtr_reactor_init( &rec );
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
}

int main( void )
{
        UNITY_BEGIN();
        RUN_TEST( test_cut_chann_id );
        return UNITY_END();
}
