#include "./util.h"

void asrtl_fill_buffer( uint8_t const* from, uint32_t from_size, uint8_t** to, uint32_t* to_size )
{
        uint32_t n = from_size > *to_size ? *to_size : from_size;
        *to_size -= n;
        for ( uint32_t i = 0; i < n; ++i )
                *( *to )++ = *from++;
}
