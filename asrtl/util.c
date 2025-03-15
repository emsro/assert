#include "./util.h"

void asrtl_fill_buffer( uint8_t const* from, uint32_t from_size, struct asrtl_span* buff )
{
        for ( uint32_t i = 0; i < from_size && buff->b != buff->e; ++i )
                *buff->b++ = *from++;
}
