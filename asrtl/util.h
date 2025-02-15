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

enum asrtl_status asrtl_cut_u16( uint8_t const** data, uint32_t* size, uint16_t* id );
enum asrtl_status asrtl_add_u16( uint8_t** data, uint32_t* size, uint16_t id );

#endif  // ASRTL_UTIL_H
