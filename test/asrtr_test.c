#include "../asrtl/core_proto.h"
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

void test_cut_u16( void )
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
}

void test_reactor_version( void )
{
        enum asrtl_status    lst;
        enum asrtr_status    st;
        struct asrtr_reactor rec;
        struct data_ll*      collected = NULL;
        struct asrtl_sender  send;
        setup_sender_collector( &send, &collected );
        asrtr_reactor_init( &rec, &send );

        uint8_t  buffer[64];
        uint8_t* p    = buffer;
        uint32_t size = sizeof buffer;
        lst           = asrtl_msg_ctor_version( &p, &size );
        TEST_ASSERT_EQUAL( lst, ASRTL_SUCCESS );

        lst = asrtr_reactor_recv( &rec, buffer, sizeof buffer - size );
        TEST_ASSERT_EQUAL( lst, ASRTL_SUCCESS );
        TEST_ASSERT_EQUAL( ASRTR_FLAG_VER, rec.flags );

        st = asrtr_reactor_tick( &rec );
        TEST_ASSERT_EQUAL( st, ASRTR_SUCCESS );
        TEST_ASSERT_EQUAL( 0x00, rec.flags );

        TEST_ASSERT_NOT_NULL( collected );
        TEST_ASSERT_EQUAL( ASRTL_CORE, collected->id );
        TEST_ASSERT_EQUAL( 0x08, collected->data_size );
        TEST_ASSERT_EQUAL( 0x00, collected->data[0] );
        TEST_ASSERT_EQUAL( ASRTL_MSG_VERSION, collected->data[1] );
        TEST_ASSERT_EQUAL( 0x00, collected->data[2] );
        TEST_ASSERT_EQUAL( 0x00, collected->data[3] );
        TEST_ASSERT_EQUAL( 0x00, collected->data[4] );
        TEST_ASSERT_EQUAL( 0x00, collected->data[5] );
        TEST_ASSERT_EQUAL( 0x00, collected->data[6] );
        TEST_ASSERT_EQUAL( 0x00, collected->data[7] );
        TEST_ASSERT_NULL( collected->next );
        free_data_ll( collected );
}

int main( void )
{
        UNITY_BEGIN();
        RUN_TEST( test_cut_u16 );
        RUN_TEST( test_reactor_version );
        return UNITY_END();
}
