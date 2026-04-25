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

#include <stddef.h>


enum asrt_status asrt_cobs_encode_buffer( struct asrt_span const in, struct asrt_span* out )
{
        ASRT_ASSERT( out && in.b && in.e && out->b && out->e );
        ASRT_ASSERT( in.e >= in.b && out->e >= out->b );
        uint8_t* out_end = out->e;

        struct asrt_cobs_encoder enc;
        asrt_cobs_encoder_init( &enc, out->b );

        for ( uint8_t* p = in.b; p != in.e; ++p ) {
                if ( enc.p + 1 >= out_end )  // iter may write 2 bytes when offset resets at 255
                        return ASRT_SIZE_ERR;
                asrt_cobs_encoder_iter( &enc, *p );
        }

        if ( enc.p >= out_end )
                return ASRT_SIZE_ERR;

        *enc.p++ = 0x00;
        out->e   = enc.p;

        return ASRT_SUCCESS;
}

enum asrt_status asrt_cobs_ibuffer_insert( struct asrt_cobs_ibuffer* b, struct asrt_span sp )
{
        ASRT_ASSERT( b && sp.b && sp.e && sp.e >= sp.b );
        ASRT_ASSERT( b->used.b <= b->used.e && b->buff.b < b->buff.e );
        ptrdiff_t s        = sp.e - sp.b;
        ptrdiff_t capacity = b->buff.e - b->used.e;
        if ( s > capacity ) {
                if ( b->used.b == b->buff.b )
                        return ASRT_SIZE_ERR;
                // shift the used buffer to the beginning, try again
                uint8_t* p = b->used.b;
                uint8_t* q = b->buff.b;
                for ( ; p != b->used.e; ++p, ++q )
                        *q = *p;
                b->used.b = b->buff.b;
                b->used.e = q;
                capacity  = b->buff.e - b->used.e;
                if ( s > capacity )
                        return ASRT_SIZE_ERR;
        }
        for ( uint8_t* p = sp.b; p != sp.e; ++p )
                *( b->used.e++ ) = *p;
        return ASRT_SUCCESS;
}

int8_t asrt_cobs_ibuffer_iter( struct asrt_cobs_ibuffer* b, struct asrt_span* buff )
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
        int capacity = (int) ( buff->e - buff->b );
        int size     = (int) ( p - b->used.b );
        if ( size > capacity )
                return -1;
        uint8_t*                 q = buff->b;
        struct asrt_cobs_decoder dec;
        asrt_cobs_decoder_init( &dec );
        for ( ; b->used.b != p; )
                asrt_cobs_decoder_iter( &dec, *( b->used.b++ ), &q );
        buff->e =
            q;  // 0-terminator intentionally left in ibuffer; see asrt_cobs_ibuffer_iter contract
        return 1;
}
