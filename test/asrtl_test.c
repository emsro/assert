#include "../asrtl/chann.h"

#include <unity.h>

void setUp( void )
{
}
void tearDown( void )
{
}

void test_cut_chann_id( void )
{
        uint8_t           data[3] = { 0x33, 0x66, 0x99 };
        enum asrtl_status st;
        uint8_t const*    p    = data;
        uint32_t          size = sizeof data;
        asrtl_chann_id    id;
        st = asrtl_cut_chann_id( &p, &size, &id );
        TEST_ASSERT_EQUAL( id, 0x3366 );
        TEST_ASSERT_EQUAL( st, ASRTL_SUCCESS );
        TEST_ASSERT_EQUAL( p, &data[2] );
        TEST_ASSERT_EQUAL( size, 1 );
}

void test_add_chann_id( void )
{
        uint8_t           data[3] = { 0x33, 0x66, 0x99 };
        enum asrtl_status st;
        uint8_t*          p    = data;
        uint32_t          size = sizeof data;
        st                     = asrtl_add_chann_id( &p, &size, 0x1122 );
        TEST_ASSERT_EQUAL( data[0], 0x11 );
        TEST_ASSERT_EQUAL( data[1], 0x22 );
        TEST_ASSERT_EQUAL( st, ASRTL_SUCCESS );
        TEST_ASSERT_EQUAL( p, &data[2] );
        TEST_ASSERT_EQUAL( size, 1 );
}

int main( void )
{
        UNITY_BEGIN();
        RUN_TEST( test_cut_chann_id );
        return UNITY_END();
}
