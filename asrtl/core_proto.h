#ifndef ASRTL_PROTO_H
#define ASRTL_PROTO_H

#include "./status.h"
#include "util.h"

#include <stdint.h>

enum asrtl_message_id_e
{
        ASRTL_MSG_LIST = 1,
};

typedef uint16_t asrtl_message_id;

static inline enum asrtl_status
asrtl_add_message_id( uint8_t** data, uint32_t* size, asrtl_message_id id )
{
        return asrtl_add_u16( data, size, id );
}
static inline enum asrtl_status
asrtl_cut_message_id( uint8_t const** data, uint32_t* size, asrtl_message_id* id )
{
        return asrtl_cut_u16( data, size, id );
}

#endif
