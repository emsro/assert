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

#include "./cobs.h"

#include "./log.h"

enum asrtl_status asrtl_cobs_encode_buffer( struct asrtl_span const in, struct asrtl_span* out )
{
        assert( out && in.b && in.e && out->b && out->e );
        assert( in.e >= in.b && out->e >= out->b );
        uint8_t* out_end = out->e;
        uint8_t* out_ptr = out->b;

        struct asrtl_cobs_encoder enc;
        asrtl_cobs_encoder_init( &enc, out_ptr );

        for ( uint8_t* p = in.b; p != in.e; ++p ) {
                if ( enc.p > out_end )
                        return ASRTL_SIZE_ERR;
                asrtl_cobs_encoder_iter( &enc, *p );
        }

        if ( enc.p >= out_end )
                return ASRTL_SIZE_ERR;

        *enc.p++ = 0x00;
        out->e   = enc.p;

        return ASRTL_SUCCESS;
}

enum asrtl_status asrtl_cobs_ibuffer_insert( struct asrtl_cobs_ibuffer* b, struct asrtl_span sp )
{
        assert( b && sp.b && sp.e && sp.e >= sp.b );
        assert( b->used.b <= b->used.e && b->buff.b < b->buff.e );
        int s        = sp.e - sp.b;
        int capacity = b->buff.e - b->used.e;
        if ( s <= capacity ) {
                for ( uint8_t* p = sp.b; p != sp.e; ++p )
                        *( b->used.e++ ) = *p;
                return ASRTL_SUCCESS;
        }
        if ( b->used.b == b->buff.b )
                return ASRTL_SIZE_ERR;

        // shift the used buffer to the beginning, try again
        uint8_t* p = b->used.b;
        uint8_t* q = b->buff.b;
        for ( ; p != b->used.e; ++p, ++q )
                *q = *p;
        b->used.b = b->buff.b;
        b->used.e = q;
        return asrtl_cobs_ibuffer_insert( b, sp );
}

int8_t asrtl_cobs_ibuffer_iter( struct asrtl_cobs_ibuffer* b, struct asrtl_span* buff )
{
        uint8_t* p = b->used.b;
        for ( ; p != b->used.e; ++p )
                if ( *p == 0 )
                        break;
        if ( p == b->used.e )  // no 0 found
                return 0;
        if ( p == b->used.b ) {  // empty
                b->used.b++;
                buff->e = buff->b;
                return 1;
        }
        int capacity = buff->e - buff->b;
        int size     = p - b->used.b;
        if ( size > capacity )
                return -1;
        uint8_t*                  q = buff->b;
        struct asrtl_cobs_decoder dec;
        asrtl_cobs_decoder_init( &dec );
        for ( ; b->used.b != p; )
                asrtl_cobs_decoder_iter( &dec, *( b->used.b++ ), &q );
        buff->e = q;
        return 1;
}
