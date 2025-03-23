
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
#ifndef ASRTL_COBS_H
#define ASRTL_COBS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <stdint.h>

// Encodes byte `b` into the buffer pointed by `*p` and updates the pointer.
// The pointer can be up to two positions - there always has to be two valid bytes in the memory.
// The pointer `*offset_p` is managed by function to update last zero offset byte.
static inline void asrtl_cobs_encode( uint8_t** offset_p, uint8_t** p, uint8_t b )
{
        if ( !*offset_p ) {
                *offset_p  = *p;
                **offset_p = 1;
                ( *p )++;
        }
        if ( **offset_p == 255 ) {
                **offset_p = 255;
                *offset_p  = ( *p )++;
                **offset_p = 1;
        }
        if ( b != 0 ) {
                **offset_p += 1;
                **p = b;
        } else {
                *offset_p  = *p;
                **offset_p = 1;
        }
        ++( *p );
}

struct asrtl_cobs_decoder
{
        uint8_t iszero;
        uint8_t offset;
};

static inline void asrtl_cobs_decoder_init( struct asrtl_cobs_decoder* e )
{
        assert( e );
        e->iszero = 0;
        e->offset = 1;
}

static inline void asrtl_cobs_decoder_iter( struct asrtl_cobs_decoder* e, uint8_t b, uint8_t** p )
{
        assert( e && p && *p );
        if ( e->offset == 1 ) {
                if ( e->iszero )
                        *( *p )++ = 0U;
                e->offset = b;
                e->iszero = b != 255U;
        } else {
                e->offset -= 1;
                *( *p )++ = b;
        }
}


#ifdef __cplusplus
}
#endif

#endif
