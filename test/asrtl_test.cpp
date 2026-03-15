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
#include "../asrtl/log.h"
#include "../asrtl/util.h"

#include <stdlib.h>
#include <string.h>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

ASRTL_DEFINE_GPOS_LOG()

TEST_CASE( "reactor_init" )
{
        uint8_t        data[3] = { 0x33, 0x66, 0x99 };
        uint8_t*       p       = data;
        asrtl_chann_id id;
        asrtl_cut_u16( &p, &id );
        CHECK_EQ( id, 0x3366 );
        CHECK_EQ( p, &data[2] );
}

TEST_CASE( "add_chann_id" )
{
        uint8_t  data[3] = { 0x33, 0x66, 0x99 };
        uint8_t* p       = data;
        asrtl_add_u16( &p, 0x1122 );
        CHECK_EQ( data[0], 0x11 );
        CHECK_EQ( data[1], 0x22 );
        CHECK_EQ( p, &data[2] );
}

TEST_CASE( "fill_buffer" )
{
        uint8_t data1[2] = { 0x01, 0x01 };
        uint8_t data2[4] = { 0x02, 0x02, 0x02, 0x02 };

        struct asrtl_span sp = { .b = data2, .e = data2 + sizeof data2 };
        asrtl_fill_buffer( data1, sizeof data1, &sp );

        CHECK_EQ( &data2[2], sp.b );
        CHECK_EQ( 0x01, data2[0] );
        CHECK_EQ( 0x01, data2[1] );
        CHECK_EQ( 0x02, data2[2] );
        CHECK_EQ( 0x02, data2[3] );

        uint8_t data3[4] = { 0x03, 0x03, 0x03, 0x03 };
        sp               = (struct asrtl_span) { .b = data1, .e = data1 + sizeof data1 };
        asrtl_fill_buffer( data3, sizeof data3, &sp );

        CHECK_EQ( &data1[2], sp.b );
        CHECK_EQ( 0x03, data1[0] );
        CHECK_EQ( 0x03, data1[1] );
}

TEST_CASE( "fill_buffer_zero_source" )
{
        uint8_t           dest[4]       = { 0xAA, 0xBB, 0xCC, 0xDD };
        struct asrtl_span sp            = { .b = dest, .e = dest + sizeof dest };
        uint8_t*          dest_b_before = sp.b;
        uint8_t*          dest_e_before = sp.e;

        asrtl_fill_buffer( NULL, 0, &sp );

        CHECK_EQ( dest_b_before, sp.b );
        CHECK_EQ( dest_e_before, sp.e );
        CHECK_EQ( 0xAA, dest[0] );
        CHECK_EQ( 0xBB, dest[1] );
        CHECK_EQ( 0xCC, dest[2] );
        CHECK_EQ( 0xDD, dest[3] );
}

TEST_CASE( "fill_buffer_zero_capacity_dest" )
{
        uint8_t           source[4] = { 0x11, 0x22, 0x33, 0x44 };
        uint8_t           dest[0];
        struct asrtl_span sp            = { .b = dest, .e = dest };
        uint8_t*          dest_b_before = sp.b;
        uint8_t*          dest_e_before = sp.e;

        asrtl_fill_buffer( source, sizeof source, &sp );

        CHECK_EQ( dest_b_before, sp.b );
        CHECK_EQ( dest_e_before, sp.e );
}

TEST_CASE( "fill_buffer_both_zero" )
{
        uint8_t           dest[0];
        struct asrtl_span sp            = { .b = dest, .e = dest };
        uint8_t*          dest_b_before = sp.b;
        uint8_t*          dest_e_before = sp.e;

        asrtl_fill_buffer( NULL, 0, &sp );

        CHECK_EQ( dest_b_before, sp.b );
        CHECK_EQ( dest_e_before, sp.e );
}

TEST_CASE( "fill_buffer_exact_fit" )
{
        uint8_t           source[3] = { 0xDE, 0xAD, 0xBE };
        uint8_t           dest[3]   = { 0x00, 0x00, 0x00 };
        struct asrtl_span sp        = { .b = dest, .e = dest + sizeof dest };

        asrtl_fill_buffer( source, sizeof source, &sp );

        CHECK_EQ( dest + 3, sp.b );
        CHECK_EQ( 0xDE, dest[0] );
        CHECK_EQ( 0xAD, dest[1] );
        CHECK_EQ( 0xBE, dest[2] );
}

struct cobs_record
{
        uint8_t const*                 raw;
        uint32_t                       raw_size;
        uint8_t const*                 encoded;
        uint32_t                       encoded_size;
        std::unique_ptr< cobs_record > next;
};

void cobs_record_append(
    std::unique_ptr< cobs_record >& head,
    uint8_t const*                  raw,
    uint32_t                        raw_size,
    uint8_t const*                  encoded,
    uint32_t                        encoded_size )
{
        struct cobs_record* node = new cobs_record{};
        node->raw_size           = raw_size;
        node->raw                = raw;
        node->encoded_size       = encoded_size;
        node->encoded            = encoded;
        node->next               = std::move( head );
        head                     = std::unique_ptr< cobs_record >( node );
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

std::unique_ptr< cobs_record > create_dataset( void )
{
        std::unique_ptr< cobs_record > head = nullptr;
        {
                static constexpr uint8_t raw[]     = { COBS_SEQ, 0xFF },
                                         encoded[] = { 0xFF, COBS_SEQ, 0x02, 0xFF, 0x00 };
                cobs_record_append( head, raw, sizeof raw, encoded, sizeof encoded );
        }
        {
                static constexpr uint8_t raw[]     = { 0x00, COBS_SEQ },
                                         encoded[] = { 0x01, 0xFF, COBS_SEQ, 0x00 };
                cobs_record_append( head, raw, sizeof raw, encoded, sizeof encoded );
        }
        {
                static constexpr uint8_t raw[] = { COBS_SEQ }, encoded[] = { 0xFF, COBS_SEQ, 0x00 };
                cobs_record_append( head, raw, sizeof raw, encoded, sizeof encoded );
        }
        {
                static constexpr uint8_t raw[]     = { 0x11, 0x00, 0x00, 0x00 },
                                         encoded[] = { 0x02, 0x11, 0x01, 0x01, 0x01, 0x00 };
                cobs_record_append( head, raw, sizeof raw, encoded, sizeof encoded );
        }
        {
                static constexpr uint8_t raw[]     = { 0x11, 0x22, 0x33, 0x44 },
                                         encoded[] = { 0x05, 0x11, 0x22, 0x33, 0x44, 0x00 };
                cobs_record_append( head, raw, sizeof raw, encoded, sizeof encoded );
        }
        {
                static constexpr uint8_t raw[]     = { 0x11, 0x22, 0x00, 0x33 },
                                         encoded[] = { 0x03, 0x11, 0x22, 0x02, 0x33, 0x00 };
                cobs_record_append( head, raw, sizeof raw, encoded, sizeof encoded );
        }
        {
                static constexpr uint8_t raw[]     = { 0x00, 0x11, 0x00 },
                                         encoded[] = { 0x01, 0x02, 0x11, 0x01, 0x00 };
                cobs_record_append( head, raw, sizeof raw, encoded, sizeof encoded );
        }
        {
                static constexpr uint8_t raw[]     = { 0x00, 0x00 },
                                         encoded[] = { 0x01, 0x01, 0x01, 0x00 };
                cobs_record_append( head, raw, sizeof raw, encoded, sizeof encoded );
        }
        {
                static constexpr uint8_t raw[] = { 0x00 }, encoded[] = { 0x01, 0x01, 0x00 };
                cobs_record_append( head, raw, sizeof raw, encoded, sizeof encoded );
        }
        return head;
}

TEST_CASE( "cobs" )
{
        std::unique_ptr< cobs_record > head = create_dataset();

        for ( auto* node = head.get(); node; node = node->next.get() ) {
                uint8_t                   buffer[512];
                struct asrtl_cobs_encoder enc;
                asrtl_cobs_encoder_init( &enc, buffer );
                for ( uint32_t i = 0; i < node->raw_size; i++ )
                        asrtl_cobs_encoder_iter( &enc, node->raw[i] );
                *enc.p++ = 0x00;
                CHECK_EQ( node->encoded_size, enc.p - buffer );
                CHECK( memcmp( node->encoded, buffer, node->encoded_size ) == 0 );
        }

        for ( auto* node = head.get(); node; node = node->next.get() ) {
                struct asrtl_cobs_decoder dec;
                uint8_t                   buffer[512];
                asrtl_cobs_decoder_init( &dec );
                uint8_t* p = buffer;
                uint8_t* e = buffer + sizeof buffer;
                for ( uint32_t i = 0; i < node->encoded_size - 1; ++i ) {
                        asrtl_cobs_decoder_iter( &dec, node->encoded[i], &p );
                        if ( p == e )
                                FAIL_CHECK( "" );
                }
                CHECK_EQ( node->raw_size, p - buffer );
                CHECK( memcmp( node->raw, buffer, node->raw_size ) == 0 );
        }
}

static size_t cobs_encode_payload(
    uint8_t const* raw,
    size_t         raw_size,
    uint8_t*       out,
    size_t         out_cap )
{
        struct asrtl_cobs_encoder enc;
        asrtl_cobs_encoder_init( &enc, out );
        for ( size_t i = 0; i < raw_size; ++i )
                asrtl_cobs_encoder_iter( &enc, raw[i] );
        *enc.p++ = 0x00;
        CHECK( (size_t) ( enc.p - out ) <= out_cap );
        return (size_t) ( enc.p - out );
}

static void cobs_ibuffer_prime(
    struct asrtl_cobs_ibuffer* ib,
    uint8_t*                   storage,
    size_t                     storage_size )
{
        struct asrtl_span sp = { .b = storage, .e = storage + storage_size };
        asrtl_cobs_ibuffer_init( ib, sp );
        ib->used.b = storage;
        ib->used.e = storage + 1;
        storage[0] = 0xAA;
}

TEST_CASE( "cobs_ibuffer_iter_no_message" )
{
        uint8_t                   storage[16] = { 0x03, 0x11, 0x22 };
        struct asrtl_cobs_ibuffer ib;
        struct asrtl_span         sp = { .b = storage, .e = storage + sizeof storage };
        asrtl_cobs_ibuffer_init( &ib, sp );
        ib.used.b = storage;
        ib.used.e = storage + 3;

        uint8_t           out[8]       = { 0xFF, 0xFF, 0xFF, 0xFF };
        struct asrtl_span out_sp       = { .b = out, .e = out + sizeof out };
        uint8_t*          out_b_before = out_sp.b;
        uint8_t*          out_e_before = out_sp.e;

        CHECK_EQ( 0, asrtl_cobs_ibuffer_iter( &ib, &out_sp ) );
        CHECK_EQ( out_b_before, out_sp.b );
        CHECK_EQ( out_e_before, out_sp.e );
}

TEST_CASE( "cobs_ibuffer_iter_single_message" )
{
        uint8_t raw[] = { 0x11, 0x22, 0x00, 0x33 };
        uint8_t encoded[32];
        size_t  enc_len = cobs_encode_payload( raw, sizeof raw, encoded, sizeof encoded );

        struct asrtl_cobs_ibuffer ib;
        struct asrtl_span         sp = { .b = encoded, .e = encoded + enc_len };
        asrtl_cobs_ibuffer_init( &ib, sp );
        ib.used.b = encoded;
        ib.used.e = encoded + enc_len;

        uint8_t           out[16];
        struct asrtl_span out_sp = { .b = out, .e = out + sizeof out };

        CHECK_EQ( 1, asrtl_cobs_ibuffer_iter( &ib, &out_sp ) );
        CHECK_EQ( sizeof raw, (size_t) ( out_sp.e - out_sp.b ) );
        CHECK( memcmp( raw, out, sizeof raw ) == 0 );
        CHECK_EQ( encoded + ( enc_len - 1 ), ib.used.b );
}

TEST_CASE( "cobs_ibuffer_iter_zero_length_message" )
{
        uint8_t                   storage[4] = { 0x00 };
        struct asrtl_cobs_ibuffer ib;
        struct asrtl_span         sp = { .b = storage, .e = storage + 1 };
        asrtl_cobs_ibuffer_init( &ib, sp );
        ib.used.b = storage;
        ib.used.e = storage + 1;

        uint8_t           out[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
        struct asrtl_span out_sp = { .b = out, .e = out + sizeof out };

        CHECK_EQ( 1, asrtl_cobs_ibuffer_iter( &ib, &out_sp ) );
        CHECK_EQ( 0, (int) ( out_sp.e - out_sp.b ) );
        CHECK_EQ( storage + 1, ib.used.b );
}

TEST_CASE( "cobs_ibuffer_iter_buffer_too_small" )
{
        uint8_t raw[] = { 0x11, 0x22, 0x33, 0x44 };
        uint8_t encoded[32];
        size_t  enc_len = cobs_encode_payload( raw, sizeof raw, encoded, sizeof encoded );

        struct asrtl_cobs_ibuffer ib;
        struct asrtl_span         sp = { .b = encoded, .e = encoded + enc_len };
        asrtl_cobs_ibuffer_init( &ib, sp );
        ib.used.b = encoded;
        ib.used.e = encoded + enc_len;

        uint8_t           out[2];
        struct asrtl_span out_sp       = { .b = out, .e = out + sizeof out };
        uint8_t*          out_b_before = out_sp.b;
        uint8_t*          out_e_before = out_sp.e;

        CHECK_EQ( -1, asrtl_cobs_ibuffer_iter( &ib, &out_sp ) );
        CHECK_EQ( out_b_before, out_sp.b );
        CHECK_EQ( out_e_before, out_sp.e );
}

TEST_CASE( "cobs_ibuffer_insert_fits_capacity" )
{
        uint8_t                   storage[16] = { 0 };
        struct asrtl_cobs_ibuffer ib;
        cobs_ibuffer_prime( &ib, storage, sizeof storage );

        uint8_t           payload[] = { 0x10, 0x20, 0x30 };
        struct asrtl_span sp        = { .b = payload, .e = payload + sizeof payload };

        CHECK_EQ( ASRTL_SUCCESS, asrtl_cobs_ibuffer_insert( &ib, sp ) );
        CHECK_EQ( storage + 1 + sizeof payload, ib.used.e );
        CHECK_EQ( 0x10, storage[1] );
        CHECK_EQ( 0x20, storage[2] );
        CHECK_EQ( 0x30, storage[3] );
}

TEST_CASE( "cobs_ibuffer_insert_shift_then_fit" )
{
        uint8_t                   storage[8] = { 0 };
        struct asrtl_cobs_ibuffer ib;
        struct asrtl_span         sp = { .b = storage, .e = storage + sizeof storage };
        asrtl_cobs_ibuffer_init( &ib, sp );

        ib.used.b  = storage + 3;
        ib.used.e  = storage + 6;
        storage[3] = 0xAA;
        storage[4] = 0xBB;
        storage[5] = 0xCC;

        uint8_t           payload[] = { 0x01, 0x02, 0x03, 0x04 };
        struct asrtl_span ins_sp    = { .b = payload, .e = payload + sizeof payload };

        CHECK_EQ( ASRTL_SUCCESS, asrtl_cobs_ibuffer_insert( &ib, ins_sp ) );
        CHECK_EQ( storage, ib.used.b );
        CHECK_EQ( storage + 7, ib.used.e );
        CHECK_EQ( 0xAA, storage[0] );
        CHECK_EQ( 0xBB, storage[1] );
        CHECK_EQ( 0xCC, storage[2] );
        CHECK_EQ( 0x01, storage[3] );
        CHECK_EQ( 0x02, storage[4] );
        CHECK_EQ( 0x03, storage[5] );
        CHECK_EQ( 0x04, storage[6] );
}

TEST_CASE( "cobs_ibuffer_insert_shift_no_fit" )
{
        // After shifting, still not enough room — must return SIZE_ERR, not recurse infinitely
        uint8_t                   storage[6] = { 0 };
        struct asrtl_cobs_ibuffer ib;
        struct asrtl_span         sp = { .b = storage, .e = storage + sizeof storage };
        asrtl_cobs_ibuffer_init( &ib, sp );

        // used starts mid-buffer so shift is possible, but even after shift the 5-byte
        // payload won't fit in a 6-byte buffer that already holds 4 bytes
        ib.used.b  = storage + 2;
        ib.used.e  = storage + 6;
        storage[2] = 0xAA;
        storage[3] = 0xBB;
        storage[4] = 0xCC;
        storage[5] = 0xDD;

        uint8_t           payload[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };
        struct asrtl_span ins_sp    = { .b = payload, .e = payload + sizeof payload };
        CHECK_EQ( ASRTL_SIZE_ERR, asrtl_cobs_ibuffer_insert( &ib, ins_sp ) );
}

TEST_CASE( "cobs_ibuffer_insert_size_err" )
{
        uint8_t                   storage[8] = { 0 };
        struct asrtl_cobs_ibuffer ib;
        cobs_ibuffer_prime( &ib, storage, sizeof storage );

        ib.used.b  = storage;
        ib.used.e  = storage + 5;
        storage[0] = 0xAA;
        storage[1] = 0xBB;
        storage[2] = 0xCC;
        storage[3] = 0xDD;
        storage[4] = 0xEE;

        uint8_t           payload[] = { 0x01, 0x02, 0x03, 0x04 };
        struct asrtl_span sp        = { .b = payload, .e = payload + sizeof payload };

        CHECK_EQ( ASRTL_SIZE_ERR, asrtl_cobs_ibuffer_insert( &ib, sp ) );
}

TEST_CASE( "cobs_ibuffer_partial_then_complete" )
{
        uint8_t raw[] = { 0x11, 0x00, 0x22 };
        uint8_t encoded[32];
        size_t  enc_len = cobs_encode_payload( raw, sizeof raw, encoded, sizeof encoded );

        uint8_t                   storage[64] = { 0 };
        struct asrtl_cobs_ibuffer ib;
        cobs_ibuffer_prime( &ib, storage, sizeof storage );

        size_t            half = enc_len / 2;
        struct asrtl_span sp1  = { .b = encoded, .e = encoded + half };
        CHECK_EQ( ASRTL_SUCCESS, asrtl_cobs_ibuffer_insert( &ib, sp1 ) );

        ib.used.b = storage + 1;

        uint8_t           out[16]      = { 0 };
        struct asrtl_span out_sp       = { .b = out, .e = out + sizeof out };
        uint8_t*          out_b_before = out_sp.b;
        uint8_t*          out_e_before = out_sp.e;

        CHECK_EQ( 0, asrtl_cobs_ibuffer_iter( &ib, &out_sp ) );
        CHECK_EQ( out_b_before, out_sp.b );
        CHECK_EQ( out_e_before, out_sp.e );

        struct asrtl_span sp2 = { .b = encoded + half, .e = encoded + enc_len };
        CHECK_EQ( ASRTL_SUCCESS, asrtl_cobs_ibuffer_insert( &ib, sp2 ) );

        CHECK_EQ( 1, asrtl_cobs_ibuffer_iter( &ib, &out_sp ) );
        CHECK_EQ( sizeof raw, (size_t) ( out_sp.e - out_sp.b ) );
        CHECK( memcmp( raw, out, sizeof raw ) == 0 );
}

TEST_CASE( "cobs_encode_buffer_success" )
{
        uint8_t raw[] = { 0x11, 0x22, 0x00, 0x33 };
        uint8_t out[16];
        uint8_t expected[32];

        struct asrtl_cobs_encoder enc;
        asrtl_cobs_encoder_init( &enc, expected );
        for ( size_t i = 0; i < sizeof raw; ++i )
                asrtl_cobs_encoder_iter( &enc, raw[i] );
        *enc.p++            = 0x00;
        size_t expected_len = enc.p - expected;

        struct asrtl_span in     = { .b = raw, .e = raw + sizeof raw };
        struct asrtl_span out_sp = { .b = out, .e = out + sizeof out };

        CHECK_EQ( ASRTL_SUCCESS, asrtl_cobs_encode_buffer( in, &out_sp ) );
        CHECK_EQ( expected_len, (size_t) ( out_sp.e - out_sp.b ) );
        CHECK( memcmp( expected, out, expected_len ) == 0 );
}

TEST_CASE( "cobs_encode_buffer_empty_input" )
{
        uint8_t raw[0];
        uint8_t out[16];

        struct asrtl_span in     = { .b = raw, .e = raw };
        struct asrtl_span out_sp = { .b = out, .e = out + sizeof out };

        CHECK_EQ( ASRTL_SUCCESS, asrtl_cobs_encode_buffer( in, &out_sp ) );
        CHECK_EQ( 2, (int) ( out_sp.e - out_sp.b ) );
        CHECK_EQ( 0x01, out[0] );
        CHECK_EQ( 0x00, out[1] );
}

TEST_CASE( "cobs_encode_buffer_insufficient_space" )
{
        uint8_t           raw[] = { 0x11, 0x22, 0x33, 0x44 };
        uint8_t           out[4];
        struct asrtl_span in     = { .b = raw, .e = raw + sizeof raw };
        struct asrtl_span out_sp = { .b = out, .e = out + sizeof out };

        CHECK_EQ( ASRTL_SIZE_ERR, asrtl_cobs_encode_buffer( in, &out_sp ) );
}

TEST_CASE( "cobs_encode_buffer_large_input" )
{
        uint8_t raw[300];
        uint8_t out[350];
        for ( size_t i = 0; i < sizeof raw; ++i )
                raw[i] = (uint8_t) ( i + 1 );

        struct asrtl_span in     = { .b = raw, .e = raw + sizeof raw };
        struct asrtl_span out_sp = { .b = out, .e = out + sizeof out };

        CHECK_EQ( ASRTL_SUCCESS, asrtl_cobs_encode_buffer( in, &out_sp ) );

        struct asrtl_cobs_decoder dec;
        uint8_t                   decoded[300];
        asrtl_cobs_decoder_init( &dec );
        uint8_t* p = decoded;
        for ( uint8_t* q = out; q < out_sp.e - 1; ++q )
                asrtl_cobs_decoder_iter( &dec, *q, &p );

        CHECK_EQ( sizeof raw, (size_t) ( p - decoded ) );
        CHECK( memcmp( raw, decoded, sizeof raw ) == 0 );
}

TEST_CASE( "cobs_ibuffer_iter_consumes_trailing_zero" )
{
        uint8_t raw[] = { 0x11, 0x22, 0x33 };
        uint8_t encoded[32];
        size_t  enc_len = cobs_encode_payload( raw, sizeof raw, encoded, sizeof encoded );

        struct asrtl_cobs_ibuffer ib;
        struct asrtl_span         sp = { .b = encoded, .e = encoded + enc_len };
        asrtl_cobs_ibuffer_init( &ib, sp );
        ib.used.b = encoded;
        ib.used.e = encoded + enc_len;

        uint8_t           out[16];
        struct asrtl_span out_sp = { .b = out, .e = out + sizeof out };

        // First call should return the message
        CHECK_EQ( 1, asrtl_cobs_ibuffer_iter( &ib, &out_sp ) );
        CHECK_EQ( sizeof raw, (size_t) ( out_sp.e - out_sp.b ) );
        CHECK( memcmp( raw, out, sizeof raw ) == 0 );

        out_sp.b = out;
        out_sp.e = out + sizeof out;
        CHECK_EQ( 1, asrtl_cobs_ibuffer_iter( &ib, &out_sp ) );

        // Verify the buffer is now empty (used.b should be past the 0)
        CHECK_EQ( encoded + enc_len, ib.used.b );
}

// ============================================================================
// Tests for asrtl_chann_cobs_dispatch
// ============================================================================

// Structure to track received messages
struct test_msg_record
{
        asrtl_chann_id chid;
        uint8_t        data[256];
        size_t         size;
};

struct test_channel_ctx
{
        struct test_msg_record messages[16];
        size_t                 msg_count;
        enum asrtl_status      return_status;
};

static enum asrtl_status test_channel_recv_cb( void* ptr, struct asrtl_span buff )
{
        struct test_channel_ctx* ctx = (struct test_channel_ctx*) ptr;
        if ( ctx->msg_count >= 16 )
                return ASRTL_RECV_INTERNAL_ERR;

        struct test_msg_record* rec = &ctx->messages[ctx->msg_count++];
        rec->size                   = buff.e - buff.b;
        if ( rec->size > sizeof( rec->data ) )
                rec->size = sizeof( rec->data );
        memcpy( rec->data, buff.b, rec->size );

        return ctx->return_status;
}

// Helper to create a COBS-encoded message with channel ID and payload
static size_t create_channel_message(
    asrtl_chann_id chid,
    uint8_t const* payload,
    size_t         payload_size,
    uint8_t*       out,
    size_t         out_cap )
{
        uint8_t  raw[2048];  // Increased to handle large payloads in tests
        uint8_t* p = raw;
        asrtl_add_u16( &p, chid );
        if ( payload && payload_size > 0 )
                memcpy( p, payload, payload_size );

        return cobs_encode_payload( raw, 2 + payload_size, out, out_cap );
}

TEST_CASE( "chann_cobs_dispatch_single_message" )
{
        struct test_channel_ctx ctx  = { .return_status = ASRTL_SUCCESS };
        struct asrtl_node       node = {
                  .chid = 42, .recv_ptr = &ctx, .recv_cb = test_channel_recv_cb, .next = NULL };

        uint8_t payload[] = { 0xAA, 0xBB, 0xCC };
        uint8_t encoded[128];
        size_t  enc_len =
            create_channel_message( 42, payload, sizeof payload, encoded, sizeof encoded );

        uint8_t                   storage[256];
        struct asrtl_cobs_ibuffer ib;
        struct asrtl_span         sp = { .b = storage, .e = storage + sizeof storage };
        asrtl_cobs_ibuffer_init( &ib, sp );

        struct asrtl_span in_sp = { .b = encoded, .e = encoded + enc_len };
        CHECK_EQ( ASRTL_SUCCESS, asrtl_chann_cobs_dispatch( &ib, &node, in_sp ) );

        CHECK_EQ( 1, ctx.msg_count );
        CHECK_EQ( sizeof payload, ctx.messages[0].size );
        CHECK( memcmp( payload, ctx.messages[0].data, sizeof payload ) == 0 );
}

TEST_CASE( "chann_cobs_dispatch_multiple_messages" )
{
        struct test_channel_ctx ctx  = { .return_status = ASRTL_SUCCESS };
        struct asrtl_node       node = {
                  .chid = 10, .recv_ptr = &ctx, .recv_cb = test_channel_recv_cb, .next = NULL };

        uint8_t msg1[] = { 0x11, 0x22 };
        uint8_t msg2[] = { 0x33, 0x44, 0x55 };
        uint8_t msg3[] = { 0x66 };

        uint8_t encoded[256];
        size_t  pos = 0;
        pos += create_channel_message( 10, msg1, sizeof msg1, encoded + pos, sizeof encoded - pos );
        pos += create_channel_message( 10, msg2, sizeof msg2, encoded + pos, sizeof encoded - pos );
        pos += create_channel_message( 10, msg3, sizeof msg3, encoded + pos, sizeof encoded - pos );

        uint8_t                   storage[512];
        struct asrtl_cobs_ibuffer ib;
        struct asrtl_span         sp = { .b = storage, .e = storage + sizeof storage };
        asrtl_cobs_ibuffer_init( &ib, sp );

        struct asrtl_span in_sp = { .b = encoded, .e = encoded + pos };
        CHECK_EQ( ASRTL_SUCCESS, asrtl_chann_cobs_dispatch( &ib, &node, in_sp ) );

        CHECK_EQ( 3, ctx.msg_count );
        CHECK_EQ( sizeof msg1, ctx.messages[0].size );
        CHECK( memcmp( msg1, ctx.messages[0].data, sizeof msg1 ) == 0 );
        CHECK_EQ( sizeof msg2, ctx.messages[1].size );
        CHECK( memcmp( msg2, ctx.messages[1].data, sizeof msg2 ) == 0 );
        CHECK_EQ( sizeof msg3, ctx.messages[2].size );
        CHECK( memcmp( msg3, ctx.messages[2].data, sizeof msg3 ) == 0 );
}

TEST_CASE( "chann_cobs_dispatch_partial_then_complete" )
{
        struct test_channel_ctx ctx  = { .return_status = ASRTL_SUCCESS };
        struct asrtl_node       node = {
                  .chid = 20, .recv_ptr = &ctx, .recv_cb = test_channel_recv_cb, .next = NULL };

        uint8_t payload[] = { 0xDE, 0xAD, 0xBE, 0xEF };
        uint8_t encoded[128];
        size_t  enc_len =
            create_channel_message( 20, payload, sizeof payload, encoded, sizeof encoded );

        uint8_t                   storage[256];
        struct asrtl_cobs_ibuffer ib;
        struct asrtl_span         sp = { .b = storage, .e = storage + sizeof storage };
        asrtl_cobs_ibuffer_init( &ib, sp );

        // Send first half
        size_t            half  = enc_len / 2;
        struct asrtl_span in_sp = { .b = encoded, .e = encoded + half };
        CHECK_EQ( ASRTL_SUCCESS, asrtl_chann_cobs_dispatch( &ib, &node, in_sp ) );
        CHECK_EQ( 0, ctx.msg_count );  // No complete message yet

        // Send second half
        in_sp.b = encoded + half;
        in_sp.e = encoded + enc_len;
        CHECK_EQ( ASRTL_SUCCESS, asrtl_chann_cobs_dispatch( &ib, &node, in_sp ) );

        CHECK_EQ( 1, ctx.msg_count );
        CHECK_EQ( sizeof payload, ctx.messages[0].size );
        CHECK( memcmp( payload, ctx.messages[0].data, sizeof payload ) == 0 );
}

TEST_CASE( "chann_cobs_dispatch_empty_payload" )
{
        struct test_channel_ctx ctx  = { .return_status = ASRTL_SUCCESS };
        struct asrtl_node       node = {
                  .chid = 99, .recv_ptr = &ctx, .recv_cb = test_channel_recv_cb, .next = NULL };

        uint8_t encoded[128];
        size_t  enc_len = create_channel_message( 99, NULL, 0, encoded, sizeof encoded );

        uint8_t                   storage[256];
        struct asrtl_cobs_ibuffer ib;
        struct asrtl_span         sp = { .b = storage, .e = storage + sizeof storage };
        asrtl_cobs_ibuffer_init( &ib, sp );

        struct asrtl_span in_sp = { .b = encoded, .e = encoded + enc_len };
        CHECK_EQ( ASRTL_SUCCESS, asrtl_chann_cobs_dispatch( &ib, &node, in_sp ) );

        CHECK_EQ( 1, ctx.msg_count );
        CHECK_EQ( 0, ctx.messages[0].size );
}

TEST_CASE( "chann_cobs_dispatch_multiple_channels" )
{
        struct test_channel_ctx ctx1 = { .return_status = ASRTL_SUCCESS };
        struct test_channel_ctx ctx2 = { .return_status = ASRTL_SUCCESS };
        struct test_channel_ctx ctx3 = { .return_status = ASRTL_SUCCESS };

        struct asrtl_node node3 = {
            .chid = 30, .recv_ptr = &ctx3, .recv_cb = test_channel_recv_cb, .next = NULL };
        struct asrtl_node node2 = {
            .chid = 20, .recv_ptr = &ctx2, .recv_cb = test_channel_recv_cb, .next = &node3 };
        struct asrtl_node node1 = {
            .chid = 10, .recv_ptr = &ctx1, .recv_cb = test_channel_recv_cb, .next = &node2 };

        uint8_t payload1[] = { 0x01 };
        uint8_t payload2[] = { 0x02, 0x02 };
        uint8_t payload3[] = { 0x03, 0x03, 0x03 };

        uint8_t encoded[256];
        size_t  pos = 0;
        pos += create_channel_message(
            10, payload1, sizeof payload1, encoded + pos, sizeof encoded - pos );
        pos += create_channel_message(
            30, payload3, sizeof payload3, encoded + pos, sizeof encoded - pos );
        pos += create_channel_message(
            20, payload2, sizeof payload2, encoded + pos, sizeof encoded - pos );

        uint8_t                   storage[512];
        struct asrtl_cobs_ibuffer ib;
        struct asrtl_span         sp = { .b = storage, .e = storage + sizeof storage };
        asrtl_cobs_ibuffer_init( &ib, sp );

        struct asrtl_span in_sp = { .b = encoded, .e = encoded + pos };
        CHECK_EQ( ASRTL_SUCCESS, asrtl_chann_cobs_dispatch( &ib, &node1, in_sp ) );

        CHECK_EQ( 1, ctx1.msg_count );
        CHECK( memcmp( payload1, ctx1.messages[0].data, sizeof payload1 ) == 0 );
        CHECK_EQ( 1, ctx2.msg_count );
        CHECK( memcmp( payload2, ctx2.messages[0].data, sizeof payload2 ) == 0 );
        CHECK_EQ( 1, ctx3.msg_count );
        CHECK( memcmp( payload3, ctx3.messages[0].data, sizeof payload3 ) == 0 );
}

TEST_CASE( "chann_cobs_dispatch_unknown_channel" )
{
        struct test_channel_ctx ctx  = { .return_status = ASRTL_SUCCESS };
        struct asrtl_node       node = {
                  .chid = 42, .recv_ptr = &ctx, .recv_cb = test_channel_recv_cb, .next = NULL };

        uint8_t payload[] = { 0xAA };
        uint8_t encoded[128];
        // Send to channel 99 which doesn't exist
        size_t enc_len =
            create_channel_message( 99, payload, sizeof payload, encoded, sizeof encoded );

        uint8_t                   storage[256];
        struct asrtl_cobs_ibuffer ib;
        struct asrtl_span         sp = { .b = storage, .e = storage + sizeof storage };
        asrtl_cobs_ibuffer_init( &ib, sp );

        struct asrtl_span in_sp = { .b = encoded, .e = encoded + enc_len };
        // Channel not found — error propagated
        CHECK_EQ( ASRTL_CHANN_NOT_FOUND, asrtl_chann_cobs_dispatch( &ib, &node, in_sp ) );
        CHECK_EQ( 0, ctx.msg_count );
}

TEST_CASE( "chann_cobs_dispatch_incremental_state" )
{
        struct test_channel_ctx ctx  = { .return_status = ASRTL_SUCCESS };
        struct asrtl_node       node = {
                  .chid = 7, .recv_ptr = &ctx, .recv_cb = test_channel_recv_cb, .next = NULL };

        uint8_t msg1[] = { 0x11 };
        uint8_t msg2[] = { 0x22, 0x22 };
        uint8_t msg3[] = { 0x33, 0x33, 0x33 };

        uint8_t enc1[64], enc2[64], enc3[64];
        size_t  len1 = create_channel_message( 7, msg1, sizeof msg1, enc1, sizeof enc1 );
        size_t  len2 = create_channel_message( 7, msg2, sizeof msg2, enc2, sizeof enc2 );
        size_t  len3 = create_channel_message( 7, msg3, sizeof msg3, enc3, sizeof enc3 );

        uint8_t                   storage[512];
        struct asrtl_cobs_ibuffer ib;
        struct asrtl_span         sp = { .b = storage, .e = storage + sizeof storage };
        asrtl_cobs_ibuffer_init( &ib, sp );

        // Process messages one at a time, maintaining state
        struct asrtl_span in_sp1 = { .b = enc1, .e = enc1 + len1 };
        CHECK_EQ( ASRTL_SUCCESS, asrtl_chann_cobs_dispatch( &ib, &node, in_sp1 ) );
        CHECK_EQ( 1, ctx.msg_count );

        struct asrtl_span in_sp2 = { .b = enc2, .e = enc2 + len2 };
        CHECK_EQ( ASRTL_SUCCESS, asrtl_chann_cobs_dispatch( &ib, &node, in_sp2 ) );
        CHECK_EQ( 2, ctx.msg_count );

        struct asrtl_span in_sp3 = { .b = enc3, .e = enc3 + len3 };
        CHECK_EQ( ASRTL_SUCCESS, asrtl_chann_cobs_dispatch( &ib, &node, in_sp3 ) );
        CHECK_EQ( 3, ctx.msg_count );

        CHECK( memcmp( msg1, ctx.messages[0].data, sizeof msg1 ) == 0 );
        CHECK( memcmp( msg2, ctx.messages[1].data, sizeof msg2 ) == 0 );
        CHECK( memcmp( msg3, ctx.messages[2].data, sizeof msg3 ) == 0 );
}

TEST_CASE( "chann_cobs_dispatch_message_too_large" )
{
        struct test_channel_ctx ctx  = { .return_status = ASRTL_SUCCESS };
        struct asrtl_node       node = {
                  .chid = 5, .recv_ptr = &ctx, .recv_cb = test_channel_recv_cb, .next = NULL };

        // Create a message larger than the internal 1024-byte buffer
        uint8_t large_payload[1200];
        for ( size_t i = 0; i < sizeof large_payload; i++ )
                large_payload[i] = (uint8_t) ( i & 0xFF );

        uint8_t encoded[2048];
        size_t  enc_len = create_channel_message(
            5, large_payload, sizeof large_payload, encoded, sizeof encoded );

        uint8_t                   storage[2048];
        struct asrtl_cobs_ibuffer ib;
        struct asrtl_span         sp = { .b = storage, .e = storage + sizeof storage };
        asrtl_cobs_ibuffer_init( &ib, sp );

        struct asrtl_span in_sp = { .b = encoded, .e = encoded + enc_len };
        // Should return size error because message is too large for internal buffer
        CHECK_EQ( ASRTL_SIZE_ERR, asrtl_chann_cobs_dispatch( &ib, &node, in_sp ) );
        CHECK_EQ( 0, ctx.msg_count );
}

TEST_CASE( "chann_cobs_dispatch_mixed_partial_and_complete" )
{
        struct test_channel_ctx ctx  = { .return_status = ASRTL_SUCCESS };
        struct asrtl_node       node = {
                  .chid = 15, .recv_ptr = &ctx, .recv_cb = test_channel_recv_cb, .next = NULL };

        uint8_t msg1[] = { 0xF1 };
        uint8_t msg2[] = { 0xF2, 0xF2 };
        uint8_t msg3[] = { 0xF3, 0xF3, 0xF3 };

        uint8_t encoded[256];
        size_t  pos = 0;
        pos += create_channel_message( 15, msg1, sizeof msg1, encoded + pos, sizeof encoded - pos );
        size_t split_point = pos + 3;  // Split msg2 partway through
        pos += create_channel_message( 15, msg2, sizeof msg2, encoded + pos, sizeof encoded - pos );
        pos += create_channel_message( 15, msg3, sizeof msg3, encoded + pos, sizeof encoded - pos );

        uint8_t                   storage[512];
        struct asrtl_cobs_ibuffer ib;
        struct asrtl_span         sp = { .b = storage, .e = storage + sizeof storage };
        asrtl_cobs_ibuffer_init( &ib, sp );

        // First call: complete msg1 + partial msg2
        struct asrtl_span in_sp = { .b = encoded, .e = encoded + split_point };
        CHECK_EQ( ASRTL_SUCCESS, asrtl_chann_cobs_dispatch( &ib, &node, in_sp ) );
        CHECK_EQ( 1, ctx.msg_count );  // Only msg1
        CHECK( memcmp( msg1, ctx.messages[0].data, sizeof msg1 ) == 0 );

        // Second call: rest of msg2 + msg3
        in_sp.b = encoded + split_point;
        in_sp.e = encoded + pos;
        CHECK_EQ( ASRTL_SUCCESS, asrtl_chann_cobs_dispatch( &ib, &node, in_sp ) );
        CHECK_EQ( 3, ctx.msg_count );  // msg1, msg2, msg3
        CHECK( memcmp( msg2, ctx.messages[1].data, sizeof msg2 ) == 0 );
        CHECK( memcmp( msg3, ctx.messages[2].data, sizeof msg3 ) == 0 );
}

TEST_CASE( "chann_cobs_dispatch_byte_by_byte" )
{
        struct test_channel_ctx ctx  = { .return_status = ASRTL_SUCCESS };
        struct asrtl_node       node = {
                  .chid = 88, .recv_ptr = &ctx, .recv_cb = test_channel_recv_cb, .next = NULL };

        uint8_t payload[] = { 0xCA, 0xFE };
        uint8_t encoded[128];
        size_t  enc_len =
            create_channel_message( 88, payload, sizeof payload, encoded, sizeof encoded );

        uint8_t                   storage[256];
        struct asrtl_cobs_ibuffer ib;
        struct asrtl_span         sp = { .b = storage, .e = storage + sizeof storage };
        asrtl_cobs_ibuffer_init( &ib, sp );

        // Feed data byte by byte
        for ( size_t i = 0; i < enc_len; i++ ) {
                struct asrtl_span in_sp = { .b = encoded + i, .e = encoded + i + 1 };
                CHECK_EQ( ASRTL_SUCCESS, asrtl_chann_cobs_dispatch( &ib, &node, in_sp ) );
        }

        CHECK_EQ( 1, ctx.msg_count );
        CHECK_EQ( sizeof payload, ctx.messages[0].size );
        CHECK( memcmp( payload, ctx.messages[0].data, sizeof payload ) == 0 );
}

TEST_CASE( "chann_cobs_dispatch_ibuffer_size_err" )
{
        // ibuffer too small for the incoming encoded message — insert must return SIZE_ERR
        // and cobs_dispatch must propagate it rather than silently succeed
        struct test_channel_ctx ctx  = { .return_status = ASRTL_SUCCESS };
        struct asrtl_node       node = {
                  .chid = 1, .recv_ptr = &ctx, .recv_cb = test_channel_recv_cb, .next = NULL };

        uint8_t payload[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };
        uint8_t encoded[128];
        size_t  enc_len =
            create_channel_message( 1, payload, sizeof payload, encoded, sizeof encoded );

        // ibuffer only 4 bytes — far too small for the encoded message
        uint8_t                   storage[4];
        struct asrtl_cobs_ibuffer ib;
        struct asrtl_span         sp = { .b = storage, .e = storage + sizeof storage };
        asrtl_cobs_ibuffer_init( &ib, sp );

        struct asrtl_span in_sp = { .b = encoded, .e = encoded + enc_len };
        CHECK_NE( ASRTL_SUCCESS, asrtl_chann_cobs_dispatch( &ib, &node, in_sp ) );
}

TEST_CASE( "chann_cobs_dispatch_recv_cb_error" )
{
        // When recv_cb returns an error, cobs_dispatch must propagate it
        struct test_channel_ctx ctx  = { .return_status = ASRTL_RECV_INTERNAL_ERR };
        struct asrtl_node       node = {
                  .chid = 1, .recv_ptr = &ctx, .recv_cb = test_channel_recv_cb, .next = NULL };

        uint8_t payload[] = { 0xAB };
        uint8_t encoded[64];
        size_t  enc_len =
            create_channel_message( 1, payload, sizeof payload, encoded, sizeof encoded );

        uint8_t                   storage[256];
        struct asrtl_cobs_ibuffer ib;
        struct asrtl_span         sp = { .b = storage, .e = storage + sizeof storage };
        asrtl_cobs_ibuffer_init( &ib, sp );

        struct asrtl_span in_sp = { .b = encoded, .e = encoded + enc_len };
        CHECK_NE( ASRTL_SUCCESS, asrtl_chann_cobs_dispatch( &ib, &node, in_sp ) );
}

TEST_CASE( "u8d4_to_u32_high_bit" )
{
        uint8_t  data[4] = { 0x80, 0x00, 0x00, 0x01 };
        uint32_t val     = 0;
        asrtl_u8d4_to_u32( data, &val );
        CHECK_EQ( 0x80000001, val );
}

TEST_CASE( "cobs_encode_buffer_l11" )
{
        // L11: Verify asrtl_cobs_encode_buffer works correctly (out_ptr is dead code)
        uint8_t           input[] = { 0x11, 0x22, 0x00, 0x33, 0x44 };
        uint8_t           output[32];
        struct asrtl_span in  = { .b = input, .e = input + sizeof input };
        struct asrtl_span out = { .b = output, .e = output + sizeof output };

        enum asrtl_status st = asrtl_cobs_encode_buffer( in, &out );

        CHECK_EQ( ASRTL_SUCCESS, st );
        CHECK_EQ( 7, (size_t) ( out.e - out.b ) );
        CHECK_EQ( 0x03, output[0] );
        CHECK_EQ( 0x11, output[1] );
        CHECK_EQ( 0x22, output[2] );
        CHECK_EQ( 0x03, output[3] );
        CHECK_EQ( 0x33, output[4] );
        CHECK_EQ( 0x44, output[5] );
        CHECK_EQ( 0x00, output[6] );
}

TEST_CASE( "cobs_ibuffer_insert_l13" )
{
        // L13: Verify asrtl_cobs_ibuffer_insert works with larger buffers
        // (uses int instead of ptrdiff_t, but should work for reasonable sizes)
        size_t                 buf_size = 1024 * 1024;  // 1MB buffer
        std::vector< uint8_t > storage( buf_size, 0 );

        struct asrtl_cobs_ibuffer ib;
        struct asrtl_span         sp = { .b = storage.data(), .e = storage.data() + buf_size };
        asrtl_cobs_ibuffer_init( &ib, sp );

        // Insert data that's large but fits
        size_t                 insert_size = 100 * 1024;  // 100KB
        std::vector< uint8_t > payload( insert_size, 0 );
        for ( size_t i = 0; i < insert_size; i++ )
                payload[i] = (uint8_t) ( i & 0xFF );

        struct asrtl_span payload_span = { .b = payload.data(), .e = payload.data() + insert_size };
        enum asrtl_status st           = asrtl_cobs_ibuffer_insert( &ib, payload_span );

        CHECK_EQ( ASRTL_SUCCESS, st );
        CHECK_EQ( insert_size, (size_t) ( ib.used.e - ib.used.b ) );
}

// L-cov2: asrtl_span_unfit_for direct tests
TEST_CASE( "span_unfit_for" )
{
        uint8_t buf[8];

        struct asrtl_span full = { .b = buf, .e = buf + 8 };
        CHECK_EQ( 0, asrtl_span_unfit_for( &full, 0 ) );
        CHECK_EQ( 0, asrtl_span_unfit_for( &full, 1 ) );
        CHECK_EQ( 0, asrtl_span_unfit_for( &full, 8 ) );
        CHECK_NE( 0, asrtl_span_unfit_for( &full, 9 ) );

        struct asrtl_span empty = { .b = buf, .e = buf };
        CHECK_EQ( 0, asrtl_span_unfit_for( &empty, 0 ) );
        CHECK_NE( 0, asrtl_span_unfit_for( &empty, 1 ) );
}

// L-cov3: asrtl_u8d2_to_u16 with high bit set
TEST_CASE( "u8d2_to_u16_high_bit" )
{
        uint8_t  data[2] = { 0x80, 0x01 };
        uint16_t val     = 0;
        asrtl_u8d2_to_u16( data, &val );
        CHECK_EQ( 0x8001, val );
}
