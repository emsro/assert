#include "./util.h"

enum asrtl_status asrtl_cut_u16( uint8_t const** data, uint32_t* size, uint16_t* id )
{
        if ( *size < 2 )
                return ASRTL_SIZE_ERR;
        asrtl_u8d2_to_u16( *data, id );
        *data += 2;
        *size -= 2;
        return ASRTL_SUCCESS;
}
enum asrtl_status asrtl_add_u16( uint8_t** data, uint32_t* size, uint16_t id )
{
        if ( *size < 2 )
                return ASRTL_SIZE_ERR;
        asrtl_u16_to_u8d2( id, *data );
        *data += 2;
        *size -= 2;
        return ASRTL_SUCCESS;
}

void asrtl_fill_buffer( uint8_t const* from, uint32_t from_size, uint8_t** to, uint32_t* to_size )
{
        uint32_t n = from_size > *to_size ? *to_size : from_size;
        *to_size -= n;
        for ( uint32_t i = 0; i < n; ++i )
                *( *to )++ = *from++;
}
