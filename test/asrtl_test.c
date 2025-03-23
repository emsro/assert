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
#include "../asrtl/cobs.h"
#include "../asrtl/util.h"

#include <stdlib.h>
#include <string.h>
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

struct cobs_record
{
        uint8_t*            raw;
        uint32_t            raw_size;
        uint8_t*            encoded;
        uint32_t            encoded_size;
        struct cobs_record* next;
};

void cobs_record_append(
    struct cobs_record** head,
    uint8_t*             raw,
    uint32_t             raw_size,
    uint8_t*             encoded,
    uint32_t             encoded_size )
{
        struct cobs_record* node = malloc( sizeof( struct cobs_record ) );
        node->raw_size           = raw_size;
        node->raw                = malloc( node->raw_size );
        memcpy( node->raw, raw, node->raw_size );
        node->encoded_size = encoded_size;
        node->encoded      = malloc( node->encoded_size );
        memcpy( node->encoded, encoded, node->encoded_size );
        node->next = *head;
        *head      = node;
}

void cobs_rec_free( struct cobs_record* head )
{
        while ( head ) {
                struct cobs_record* next = head->next;
                free( head->raw );
                free( head->encoded );
                free( head );
                head = next;
        }
}

#define COBS_SEQ                                                                                  \
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, \
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,   \
            0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,   \
            0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,   \
            0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,   \
            0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55,   \
            0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63,   \
            0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71,   \
            0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,   \
            0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D,   \
            0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B,   \
            0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9,   \
            0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,   \
            0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5,   \
            0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3,   \
            0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1,   \
            0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,   \
            0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD,   \
            0xFE

void test_cobs( void )
{
        struct cobs_record* head = NULL;
        {
                uint8_t raw[]     = { COBS_SEQ, 0xFF },
                        encoded[] = { 0xFF, COBS_SEQ, 0x02, 0xFF, 0x00 };
                cobs_record_append( &head, raw, sizeof raw, encoded, sizeof encoded );
        }
        {
                uint8_t raw[] = { 0x00, COBS_SEQ }, encoded[] = { 0x01, 0xFF, COBS_SEQ, 0x00 };
                cobs_record_append( &head, raw, sizeof raw, encoded, sizeof encoded );
        }
        {
                uint8_t raw[] = { COBS_SEQ }, encoded[] = { 0xFF, COBS_SEQ, 0x00 };
                cobs_record_append( &head, raw, sizeof raw, encoded, sizeof encoded );
        }
        {
                uint8_t raw[]     = { 0x11, 0x00, 0x00, 0x00 },
                        encoded[] = { 0x02, 0x11, 0x01, 0x01, 0x01, 0x00 };
                cobs_record_append( &head, raw, sizeof raw, encoded, sizeof encoded );
        }
        {
                uint8_t raw[]     = { 0x11, 0x22, 0x33, 0x44 },
                        encoded[] = { 0x05, 0x11, 0x22, 0x33, 0x44, 0x00 };
                cobs_record_append( &head, raw, sizeof raw, encoded, sizeof encoded );
        }
        {
                uint8_t raw[]     = { 0x11, 0x22, 0x00, 0x33 },
                        encoded[] = { 0x03, 0x11, 0x22, 0x02, 0x33, 0x00 };
                cobs_record_append( &head, raw, sizeof raw, encoded, sizeof encoded );
        }
        {
                uint8_t raw[] = { 0x00, 0x11, 0x00 }, encoded[] = { 0x01, 0x02, 0x11, 0x01, 0x00 };
                cobs_record_append( &head, raw, sizeof raw, encoded, sizeof encoded );
        }
        {
                uint8_t raw[] = { 0x00, 0x00 }, encoded[] = { 0x01, 0x01, 0x01, 0x00 };
                cobs_record_append( &head, raw, sizeof raw, encoded, sizeof encoded );
        }
        {
                uint8_t raw[] = { 0x00 }, encoded[] = { 0x01, 0x01, 0x00 };
                cobs_record_append( &head, raw, sizeof raw, encoded, sizeof encoded );
        }

        // XXX: test corner cases
        struct cobs_record* node;
        for ( node = head; node; node = node->next ) {
                uint8_t  buffer[512];
                uint8_t *p1 = NULL, *p2 = buffer;
                for ( uint32_t i = 0; i < node->raw_size; i++ )
                        asrtl_cobs_encode( &p1, &p2, node->raw[i] );
                *p2++ = 0x00;
                TEST_ASSERT_EQUAL( node->encoded_size, p2 - buffer );
                TEST_ASSERT_EQUAL_MEMORY( node->encoded, buffer, node->encoded_size );
        }

        for ( node = head; node; node = node->next ) {
                struct asrtl_cobs_decoder dec;
                uint8_t                   buffer[512];
                asrtl_cobs_decoder_init( &dec );
                uint8_t* p = buffer;
                uint8_t* e = buffer + sizeof buffer;
                for ( uint32_t i = 0; i < node->encoded_size - 1; ++i ) {
                        asrtl_cobs_decoder_iter( &dec, node->encoded[i], &p );
                        if ( p == e )
                                TEST_FAIL();
                }
                TEST_ASSERT_EQUAL( node->raw_size, p - buffer );
                TEST_ASSERT_EQUAL_MEMORY( node->raw, buffer, node->raw_size );
        }


        cobs_rec_free( head );
}

int main( void )
{
        UNITY_BEGIN();
        RUN_TEST( test_reactor_init );
        RUN_TEST( test_add_chann_id );
        RUN_TEST( test_fill_buffer );
        RUN_TEST( test_cobs );
        return UNITY_END();
}
