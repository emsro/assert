
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

#include "./span.h"
#include "./status.h"

#include <assert.h>
#include <stdint.h>

/// Stateful COBS encoder.
struct asrtl_cobs_encoder
{
        uint8_t* offset;
        uint8_t* p;
};

/// Initializes COBS encoding state. Must be called before `asrtl_cobs_encoder_iter`.
static inline void asrtl_cobs_encoder_init( struct asrtl_cobs_encoder* e, uint8_t* buffer )
{
        assert( e );
        e->offset  = buffer;
        e->p       = buffer;
        *e->offset = 1;
        e->p++;
}

/// Encodes a single byte. Buffer must have at least 2 bytes available from `e->p`.
static inline void asrtl_cobs_encoder_iter( struct asrtl_cobs_encoder* e, uint8_t b )
{
        assert( e );
        if ( *e->offset == 255 ) {
                *e->offset = 255;
                e->offset  = e->p++;
                *e->offset = 1;
        }
        if ( b != 0 ) {
                *e->offset += 1;
                *e->p = b;
        } else {
                e->offset  = e->p;
                *e->offset = 1;
        }
        e->p++;
}

/// Encodes entire buffer with COBS. Returns encoded length in `out->e - out->b`, or error status.
/// Returns ASRTL_SIZE_ERR if output buffer is insufficient during encoding.
enum asrtl_status asrtl_cobs_encode_buffer( struct asrtl_span const in, struct asrtl_span* out );

/// Stateful COBS decoder. Maintains position within current code block.
struct asrtl_cobs_decoder
{
        uint8_t iszero;
        uint8_t offset;
};

static inline void asrtl_cobs_decoder_init( struct asrtl_cobs_decoder* d )
{
        assert( d );
        d->iszero = 0;
        d->offset = 1;
}

/// Decodes one COBS-encoded byte. May write zero or one byte to `*p`.
static inline void asrtl_cobs_decoder_iter( struct asrtl_cobs_decoder* d, uint8_t b, uint8_t** p )
{
        assert( d );
        if ( d->offset == 1 ) {
                if ( d->iszero )
                        *( *p )++ = 0U;
                d->offset = b;
                d->iszero = b != 255U ? 1U : 0U;
        } else {
                d->offset -= 1;
                *( *p )++ = b;
        }
}

/// Input buffer for COBS-encoded data. `buff` is total capacity, `used` is occupied region.
struct asrtl_cobs_ibuffer
{
        struct asrtl_span buff;
        struct asrtl_span used;
};

static inline void asrtl_cobs_ibuffer_init( struct asrtl_cobs_ibuffer* b, struct asrtl_span sp )
{
        assert( b );
        b->buff = sp;
        b->used = ( struct asrtl_span ){ .b = sp.b, .e = sp.b };
}

/// Appends data to buffer. Shifts existing data to start if needed to make room.
enum asrtl_status asrtl_cobs_ibuffer_insert( struct asrtl_cobs_ibuffer* b, struct asrtl_span sp );

/// Decodes next COBS message from buffer. Returns: 1 (success, sets `buff->e`), 0 (incomplete or
/// empty), -1 (too large). buff contains the decoded message on success.
int8_t asrtl_cobs_ibuffer_iter( struct asrtl_cobs_ibuffer* b, struct asrtl_span* buff );

#ifdef __cplusplus
}
#endif

#endif
