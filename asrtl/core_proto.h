#ifndef ASRTL_PROTO_H
#define ASRTL_PROTO_H

#include "./status.h"
#include "util.h"

#include <stdint.h>

enum asrtl_message_id_e
{
        ASRTL_MSG_VERSION    = 1,
        ASRTL_MSG_DESC       = 2,
        ASRTL_MSG_TEST_COUNT = 3,
        ASRTL_MSG_TEST_INFO  = 4,
};

typedef uint16_t asrtl_message_id;

static inline enum asrtl_status asrtl_msg_rtoc_version(
    uint8_t** data,
    uint32_t* size,
    uint16_t  major,
    uint16_t  minor,
    uint16_t  patch )
{
        if ( *size < sizeof( asrtl_message_id ) + 3 * sizeof( uint16_t ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( data, size, ASRTL_MSG_VERSION );
        asrtl_add_u16( data, size, major );
        asrtl_add_u16( data, size, minor );
        asrtl_add_u16( data, size, patch );
        return ASRTL_SUCCESS;
}
static inline enum asrtl_status asrtl_msg_ctor_version( uint8_t** data, uint32_t* size )
{
        if ( *size < sizeof( asrtl_message_id ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( data, size, ASRTL_MSG_VERSION );
        return ASRTL_SUCCESS;
}

static inline enum asrtl_status
asrtl_msg_rtoc_desc( uint8_t** data, uint32_t* size, char const* desc, uint32_t desc_size )
{
        if ( *size < sizeof( asrtl_message_id ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( data, size, ASRTL_MSG_DESC );
        asrtl_fill_buffer( (uint8_t const*) desc, desc_size, data, size );
        return ASRTL_SUCCESS;
}

static inline enum asrtl_status asrtl_msg_ctor_desc( uint8_t** data, uint32_t* size )
{
        if ( *size < sizeof( asrtl_message_id ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( data, size, ASRTL_MSG_DESC );
        return ASRTL_SUCCESS;
}

static inline enum asrtl_status
asrtl_msg_rtoc_count( uint8_t** data, uint32_t* size, uint16_t count )
{
        if ( *size < sizeof( asrtl_message_id ) + sizeof count )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( data, size, ASRTL_MSG_TEST_COUNT );
        asrtl_add_u16( data, size, count );
        return ASRTL_SUCCESS;
}

static inline enum asrtl_status asrtl_msg_ctor_test_count( uint8_t** data, uint32_t* size )
{
        if ( *size < sizeof( asrtl_message_id ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( data, size, ASRTL_MSG_TEST_COUNT );
        return ASRTL_SUCCESS;
}

static inline enum asrtl_status asrtl_msg_rtoc_test_info(
    uint8_t** data,
    uint32_t* size,
    uint16_t  id,
    char*     name,
    uint32_t  name_size )
{
        if ( *size < sizeof( asrtl_message_id ) + sizeof( uint16_t ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( data, size, ASRTL_MSG_TEST_INFO );
        asrtl_add_u16( data, size, id );
        asrtl_fill_buffer( (uint8_t const*) name, name_size, data, size );
        return ASRTL_SUCCESS;
}

#endif
