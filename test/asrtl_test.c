/// Permission to use, copy, modify, and/or distribute this software for any
/// purpose with or without fee is hereby granted.
///
/// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
/// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
/// AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
/// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
/// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
/// OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
/// PERFORMANCE OF THIS SOFTWARE.
#include "../asrtl/chann.h"

#include <unity.h>

void setUp( void )
{
}
void tearDown( void )
{
}

void test_reactor_init( void )
{
        uint8_t        data[3] = { 0x33, 0x66, 0x99 };
        uint8_t*       p       = data;
        asrtl_chann_id id;
        asrtl_cut_u16( &p, &id );
        TEST_ASSERT_EQUAL( id, 0x3366 );
        TEST_ASSERT_EQUAL( p, &data[2] );
}

void test_add_chann_id( void )
{
        uint8_t  data[3] = { 0x33, 0x66, 0x99 };
        uint8_t* p       = data;
        asrtl_add_u16( &p, 0x1122 );
        TEST_ASSERT_EQUAL( data[0], 0x11 );
        TEST_ASSERT_EQUAL( data[1], 0x22 );
        TEST_ASSERT_EQUAL( p, &data[2] );
}

void test_fill_buffer( void )
{
        uint8_t data1[2] = { 0x01, 0x01 };
        uint8_t data2[4] = { 0x02, 0x02, 0x02, 0x02 };

        struct asrtl_span sp = { .b = data2, .e = data2 + sizeof data2 };
        asrtl_fill_buffer( data1, sizeof data1, &sp );

        TEST_ASSERT_EQUAL( &data2[2], sp.b );
        TEST_ASSERT_EQUAL( 0x01, data2[0] );
        TEST_ASSERT_EQUAL( 0x01, data2[1] );
        TEST_ASSERT_EQUAL( 0x02, data2[2] );
        TEST_ASSERT_EQUAL( 0x02, data2[3] );

        uint8_t data3[4] = { 0x03, 0x03, 0x03, 0x03 };
        sp               = ( struct asrtl_span ){ .b = data1, .e = data1 + sizeof data1 };
        asrtl_fill_buffer( data3, sizeof data3, &sp );

        TEST_ASSERT_EQUAL( &data1[2], sp.b );
        TEST_ASSERT_EQUAL( 0x03, data1[0] );
        TEST_ASSERT_EQUAL( 0x03, data1[1] );
}

int main( void )
{
        UNITY_BEGIN();
        RUN_TEST( test_reactor_init );
        RUN_TEST( test_add_chann_id );
        RUN_TEST( test_fill_buffer );
        return UNITY_END();
}
