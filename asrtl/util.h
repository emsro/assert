#ifndef ASRTL_UTIL_H
#define ASRTL_UTIL_H

#include "status.h"

#include <stdint.h>

static inline void asrtl_u16_to_u8d2( uint16_t val, uint8_t* data )
{
        data[0] = ( val >> 8 ) & 0xFF;
        data[1] = val & 0xFF;
}
static inline void asrtl_u8d2_to_u16( uint8_t const* data, uint16_t* val )
{
        *val = ( data[0] << 8 ) + data[1];
}

static inline void asrtl_cut_u16( uint8_t const** data, uint32_t* size, uint16_t* val )
{
        asrtl_u8d2_to_u16( *data, val );
        *data += 2;
        *size -= 2;
}
static inline void asrtl_add_u16( uint8_t** data, uint32_t* size, uint16_t val )
{
        asrtl_u16_to_u8d2( val, *data );
        *data += 2;
        *size -= 2;
}

// Copy data from `from` to `to` respecting sizes of both buffers, copies only as much as possible,
// updates to/to_size to reflect filled in data
void asrtl_fill_buffer( uint8_t const* from, uint32_t from_size, uint8_t** to, uint32_t* to_size );

#endif  // ASRTL_UTIL_H
