#ifndef ASRTL_CHANN_H
#define ASRTL_CHANN_H

#include "./status.h"
#include "util.h"

#include <assert.h>
#include <stdint.h>

enum asrtl_chann_id_e
{
        ASRTL_META = 1,
        ASRTL_CORE = 2,
};

typedef uint16_t asrtl_chann_id;

typedef enum asrtl_status (
    *asrtl_rec_callback )( void* data, uint8_t const* msg, uint32_t msg_size );

struct asrtl_node
{
        asrtl_chann_id     chid;
        void*              recv_data;
        asrtl_rec_callback recv_fn;
        struct asrtl_node* next;
};

typedef enum asrtl_status (
    *asrtl_send_callback )( void* data, asrtl_chann_id id, uint8_t const* msg, uint32_t msg_size );

struct asrtl_sender
{
        void*               send_data;
        asrtl_send_callback send_fn;
};

static inline enum asrtl_status
asrtl_send( struct asrtl_sender* r, asrtl_chann_id chid, uint8_t const* msg, uint32_t msg_size )
{
        assert( r );
        return r->send_fn( r->send_data, chid, msg, msg_size );
}

static inline enum asrtl_status
asrtl_cut_chann_id( uint8_t const** data, uint32_t* size, asrtl_chann_id* id )
{
        return asrtl_cut_u16( data, size, id );
}
static inline enum asrtl_status
asrtl_add_chann_id( uint8_t** data, uint32_t* size, asrtl_chann_id id )
{
        return asrtl_add_u16( data, size, id );
}

#endif
