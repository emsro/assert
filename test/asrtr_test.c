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

void test_reactor_init( void )
{
        struct asrtr_reactor rec;
        struct data_ll*      collected = NULL;
        struct asrtl_sender  send;
        setup_sender_collector( &send, &collected );

        asrtr_reactor_init( &rec, &send, "rec1" );
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

#define COMBINE1( X, Y ) X##Y  // helper macro
#define COMBINE( X, Y ) COMBINE1( X, Y )

#define DO_REACTOR_RECV_FLAG( rec, buffer, size, flag )                                         \
        enum asrtl_status COMBINE( st_l, __LINE__ ) = asrtr_reactor_recv( &rec, buffer, size ); \
        TEST_ASSERT_EQUAL( ASRTL_SUCCESS, COMBINE( st_l, __LINE__ ) );                          \
        TEST_ASSERT_EQUAL( flag, rec.flags );

#define DO_REACTOR_TICK_NOFLAG( rec )                                             \
        enum asrtr_status COMBINE( st_l, __LINE__ ) = asrtr_reactor_tick( &rec ); \
        TEST_ASSERT_EQUAL( ASRTR_SUCCESS, COMBINE( st_l, __LINE__ ) );            \
        TEST_ASSERT_EQUAL( 0x00, rec.flags );

#define CHECK_COLLECTED_HDR( collected, size )          \
        TEST_ASSERT_NOT_NULL( collected );              \
        TEST_ASSERT_EQUAL( ASRTL_CORE, collected->id ); \
        TEST_ASSERT_EQUAL( size, collected->data_size );

#define CHECK_U16( val, data )                                 \
        TEST_ASSERT_EQUAL( ( val >> 8 ) & 0xFF, ( data )[0] ); \
        TEST_ASSERT_EQUAL( val & 0xFF, ( data )[1] );

#define CLEAR_SINGLE_COLLECTED( collected )  \
        TEST_ASSERT_NULL( collected->next ); \
        free_data_ll( collected );           \
        collected = NULL;

void test_reactor_version( void )
{
        struct asrtr_reactor rec;
        struct data_ll*      collected = NULL;
        struct asrtl_sender  send;
        setup_sender_collector( &send, &collected );
        asrtr_reactor_init( &rec, &send, "rec1" );

        uint8_t  buffer[64];
        uint8_t* p    = buffer;
        uint32_t size = sizeof buffer;
        asrtl_msg_ctor_version( &p, &size );

        DO_REACTOR_RECV_FLAG( rec, buffer, sizeof buffer - size, ASRTR_FLAG_VER );

        DO_REACTOR_TICK_NOFLAG( rec );

        CHECK_COLLECTED_HDR( collected, 0x08 );
        CHECK_U16( ASRTL_MSG_VERSION, collected->data );
        CHECK_U16( 0x00, collected->data + 2 );
        CHECK_U16( 0x00, collected->data + 4 );
        CHECK_U16( 0x00, collected->data + 6 );

        CLEAR_SINGLE_COLLECTED( collected );
}

void test_reactor_desc( void )
{
        struct asrtr_reactor rec;
        struct data_ll*      collected = NULL;
        struct asrtl_sender  send;
        setup_sender_collector( &send, &collected );
        asrtr_reactor_init( &rec, &send, "rec1" );

        uint8_t  buffer[64];
        uint8_t* p    = buffer;
        uint32_t size = sizeof buffer;
        asrtl_msg_ctor_desc( &p, &size );

        DO_REACTOR_RECV_FLAG( rec, buffer, sizeof buffer - size, ASRTR_FLAG_DESC );

        DO_REACTOR_TICK_NOFLAG( rec );

        CHECK_COLLECTED_HDR( collected, 0x06 );
        CHECK_U16( ASRTL_MSG_DESC, collected->data );
        TEST_ASSERT_EQUAL_STRING_LEN( "rec1", &collected->data[2], collected->data_size - 2 );

        CLEAR_SINGLE_COLLECTED( collected );
}

void test_reactor_test_count( void )
{
        enum asrtr_status    st;
        struct asrtr_reactor rec;
        struct data_ll*      collected = NULL;
        struct asrtl_sender  send;
        setup_sender_collector( &send, &collected );
        asrtr_reactor_init( &rec, &send, "rec1" );

        uint8_t  buffer[64];
        uint8_t* p    = buffer;
        uint32_t size = sizeof buffer;
        asrtl_msg_ctor_test_count( &p, &size );

        DO_REACTOR_RECV_FLAG( rec, buffer, sizeof buffer - size, ASRTR_FLAG_TC );

        DO_REACTOR_TICK_NOFLAG( rec );

        CHECK_COLLECTED_HDR( collected, 0x04 );
        CHECK_U16( ASRTL_MSG_TEST_COUNT, collected->data );
        CHECK_U16( 0x00, collected->data + 2 );

        CLEAR_SINGLE_COLLECTED( collected );

        struct asrtr_test t1;
        st = asrtr_test_init( &t1, "test1", NULL, &dataless_test_fun );
        TEST_ASSERT_EQUAL( st, ASRTR_SUCCESS );
        asrtr_add_test( &rec, &t1 );

        asrtr_reactor_recv( &rec, buffer, sizeof buffer - size );
        asrtr_reactor_tick( &rec );

        CHECK_COLLECTED_HDR( collected, 0x04 );
        CHECK_U16( ASRTL_MSG_TEST_COUNT, collected->data );
        CHECK_U16( 0x01, collected->data + 2 );
        CLEAR_SINGLE_COLLECTED( collected );
}

void test_reactor_test_info( void )
{
        enum asrtr_status    st;
        struct asrtr_reactor rec;
        struct data_ll*      collected = NULL;
        struct asrtl_sender  send;
        setup_sender_collector( &send, &collected );
        asrtr_reactor_init( &rec, &send, "rec1" );

        uint8_t  buffer[64];
        uint8_t* p    = buffer;
        uint32_t size = sizeof buffer;
        asrtl_msg_ctor_test_info( &p, &size, 0 );

        DO_REACTOR_RECV_FLAG( rec, buffer, sizeof buffer - size, ASRTR_FLAG_TI );

        DO_REACTOR_TICK_NOFLAG( rec );

        CHECK_COLLECTED_HDR( collected, 0x15 );
        CHECK_U16( ASRTL_MSG_ERROR, collected->data );
        TEST_ASSERT_EQUAL_STRING_LEN(
            "Failed to find test", &collected->data[2], collected->data_size - 2 );
        CLEAR_SINGLE_COLLECTED( collected );

        struct asrtr_test t1;
        st = asrtr_test_init( &t1, "test1", NULL, &dataless_test_fun );
        TEST_ASSERT_EQUAL( st, ASRTR_SUCCESS );
        asrtr_add_test( &rec, &t1 );

        asrtr_reactor_recv( &rec, buffer, sizeof buffer - size );
        asrtr_reactor_tick( &rec );

        CHECK_COLLECTED_HDR( collected, 0x09 );
        CHECK_U16( ASRTL_MSG_TEST_INFO, collected->data );
        CHECK_U16( 0x00, collected->data + 2 );
        TEST_ASSERT_EQUAL_STRING_LEN( "test1", &collected->data[4], collected->data_size - 4 );
        CLEAR_SINGLE_COLLECTED( collected );
}


int main( void )
{
        UNITY_BEGIN();
        RUN_TEST( test_reactor_init );
        RUN_TEST( test_reactor_version );
        RUN_TEST( test_reactor_desc );
        RUN_TEST( test_reactor_test_count );
        RUN_TEST( test_reactor_test_info );
        return UNITY_END();
}
