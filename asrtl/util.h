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
#ifndef ASRT_UTIL_H
#define ASRT_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "./span.h"
#include "status.h"

#include <stdint.h>
#include <string.h>

/// Serialize @p val as two big-endian bytes into @p data[0..1].
static inline void asrt_u16_to_u8d2( uint16_t val, uint8_t* data )
{
        data[0] = ( val >> 8 ) & 0xFF;
        data[1] = val & 0xFF;
}
/// Serialize @p val as four big-endian bytes into @p data[0..3].
static inline void asrt_u32_to_u8d4( uint32_t val, uint8_t* data )
{
        data[0] = ( val >> 8 * 3 ) & 0xFF;
        data[1] = ( val >> 8 * 2 ) & 0xFF;
        data[2] = ( val >> 8 * 1 ) & 0xFF;
        data[3] = ( val >> 8 * 0 ) & 0xFF;
}
/// Deserialize two big-endian bytes from @p data[0..1] into @p *val.
static inline void asrt_u8d2_to_u16( uint8_t const* data, uint16_t* val )
{
        *val = ( data[0] << 8 ) + data[1];
}
/// Deserialize four big-endian bytes from @p data[0..3] into @p *val.
static inline void asrt_u8d4_to_u32( uint8_t const* data, uint32_t* val )
{
        *val = ( (uint32_t) data[0] << 8 * 3 ) + ( (uint32_t) data[1] << 8 * 2 ) +
               ( (uint32_t) data[2] << 8 * 1 ) + ( (uint32_t) data[3] << 8 * 0 );
}

/// Deserialize two big-endian bytes from @p *data into @p *val, then advance @p *data by 2.
static inline void asrt_cut_u16( uint8_t** data, uint16_t* val )
{
        asrt_u8d2_to_u16( *data, val );
        *data += 2;
}
/// Deserialize four big-endian bytes from @p *data into @p *val, then advance @p *data by 4.
static inline void asrt_cut_u32( uint8_t** data, uint32_t* val )
{
        asrt_u8d4_to_u32( *data, val );
        *data += 4;
}
/// Serialize @p val as two big-endian bytes at @p *data, then advance @p *data by 2.
static inline void asrt_add_u16( uint8_t** data, uint16_t val )
{
        asrt_u16_to_u8d2( val, *data );
        *data += 2;
}
/// Serialize @p val as four big-endian bytes at @p *data, then advance @p *data by 4.
static inline void asrt_add_u32( uint8_t** data, uint32_t val )
{
        asrt_u32_to_u8d4( val, *data );
        *data += 4;
}
/// Deserialize four big-endian bytes from @p *data as int32_t (via bit reinterpretation),
/// then advance @p *data by 4.
static inline void asrt_cut_i32( uint8_t** data, int32_t* val )
{
        uint32_t bits;
        asrt_u8d4_to_u32( *data, &bits );
        memcpy( val, &bits, sizeof bits );
        *data += 4;
}
/// Serialize @p val as four big-endian bytes (bit reinterpretation) at @p *data,
/// then advance @p *data by 4.
static inline void asrt_add_i32( uint8_t** data, int32_t val )
{
        uint32_t bits;
        memcpy( &bits, &val, sizeof bits );
        asrt_u32_to_u8d4( bits, *data );
        *data += 4;
}

/// Copy up to @p from_size bytes from @p from into @p buff, trimming to the
/// available capacity.  Advances @p buff->b by the number of bytes written.
void asrt_fill_buffer( uint8_t const* from, uint32_t from_size, struct asrt_span* buff );


#ifdef __cplusplus
}
#endif

#endif  // ASRT_UTIL_H
