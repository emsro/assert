#include "../asrtl/chann.h"

#include <unity.h>

void setUp( void )
{
}
void tearDown( void )
{
}

void test_cut_u16( void )
{
        uint8_t        data[3] = { 0x33, 0x66, 0x99 };
        uint8_t const* p       = data;
        uint32_t       size    = sizeof data;
        asrtl_chann_id id;
        asrtl_cut_u16( &p, &size, &id );
        TEST_ASSERT_EQUAL( id, 0x3366 );
        TEST_ASSERT_EQUAL( p, &data[2] );
        TEST_ASSERT_EQUAL( size, 1 );
}

void test_add_chann_id( void )
{
        uint8_t  data[3] = { 0x33, 0x66, 0x99 };
        uint8_t* p       = data;
        uint32_t size    = sizeof data;
        asrtl_add_u16( &p, &size, 0x1122 );
        TEST_ASSERT_EQUAL( data[0], 0x11 );
        TEST_ASSERT_EQUAL( data[1], 0x22 );
        TEST_ASSERT_EQUAL( p, &data[2] );
        TEST_ASSERT_EQUAL( size, 1 );
}

void test_fill_buffer( void )
{
        uint8_t data1[2] = { 0x01, 0x01 };
        uint8_t data2[4] = { 0x02, 0x02, 0x02, 0x02 };

        uint8_t* p    = data2;
        uint32_t size = sizeof data2;
        asrtl_fill_buffer( data1, sizeof data1, &p, &size );

        TEST_ASSERT_EQUAL( 2, size );
        TEST_ASSERT_EQUAL( &data2[2], p );
        TEST_ASSERT_EQUAL( 0x01, data2[0] );
        TEST_ASSERT_EQUAL( 0x01, data2[1] );
        TEST_ASSERT_EQUAL( 0x02, data2[2] );
        TEST_ASSERT_EQUAL( 0x02, data2[3] );

        uint8_t data3[4] = { 0x03, 0x03, 0x03, 0x03 };
        p                = data1;
        size             = sizeof data1;
        asrtl_fill_buffer( data3, sizeof data3, &p, &size );

        TEST_ASSERT_EQUAL( 0, size );
        TEST_ASSERT_EQUAL( &data1[2], p );
        TEST_ASSERT_EQUAL( 0x03, data1[0] );
        TEST_ASSERT_EQUAL( 0x03, data1[1] );
}

int main( void )
{
        UNITY_BEGIN();
        RUN_TEST( test_cut_u16 );
        RUN_TEST( test_add_chann_id );
        RUN_TEST( test_fill_buffer );
        return UNITY_END();
}
