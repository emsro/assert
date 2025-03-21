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
#ifndef ASRTL_UTIL_H
#define ASRTL_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "./span.h"
#include "status.h"

#include <stdint.h>

static inline void asrtl_u16_to_u8d2( uint16_t val, uint8_t* data )
{
        data[0] = ( val >> 8 ) & 0xFF;
        data[1] = val & 0xFF;
}
static inline void asrtl_u32_to_u8d4( uint32_t val, uint8_t* data )
{
        data[0] = ( val >> 8 * 3 ) & 0xFF;
        data[1] = ( val >> 8 * 2 ) & 0xFF;
        data[2] = ( val >> 8 * 1 ) & 0xFF;
        data[3] = ( val >> 8 * 0 ) & 0xFF;
}
static inline void asrtl_u8d2_to_u16( uint8_t const* data, uint16_t* val )
{
        *val = ( data[0] << 8 ) + data[1];
}
static inline void asrtl_u8d4_to_u32( uint8_t const* data, uint32_t* val )
{
        *val = ( data[0] << 8 * 3 ) + ( data[1] << 8 * 2 ) + ( data[2] << 8 * 1 ) +
               ( data[3] << 8 * 0 );
}

static inline void asrtl_cut_u16( uint8_t** data, uint16_t* val )
{
        asrtl_u8d2_to_u16( *data, val );
        *data += 2;
}
static inline void asrtl_cut_u32( uint8_t** data, uint32_t* val )
{
        asrtl_u8d4_to_u32( *data, val );
        *data += 4;
}
static inline void asrtl_add_u16( uint8_t** data, uint16_t val )
{
        asrtl_u16_to_u8d2( val, *data );
        *data += 2;
}
static inline void asrtl_add_u32( uint8_t** data, uint32_t val )
{
        asrtl_u32_to_u8d4( val, *data );
        *data += 4;
}

// Copy data from `from` to `to` respecting sizes of both buffers, copies only as much as possible,
// updates to/to_size to reflect filled in data
void asrtl_fill_buffer( uint8_t const* from, uint32_t from_size, struct asrtl_span* buff );

#ifdef __cplusplus
}
#endif

#endif  // ASRTL_UTIL_H
