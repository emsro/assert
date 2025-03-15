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

typedef enum asrtl_status ( *asrtl_rec_callback )( void* data, struct asrtl_span buff );

struct asrtl_node
{
        asrtl_chann_id     chid;
        void*              recv_data;
        asrtl_rec_callback recv_fn;
        struct asrtl_node* next;
};

typedef enum asrtl_status (
    *asrtl_send_callback )( void* data, asrtl_chann_id id, struct asrtl_span buff );

struct asrtl_sender
{
        void*               send_data;
        asrtl_send_callback send_fn;
};

static inline enum asrtl_status
asrtl_send( struct asrtl_sender* r, asrtl_chann_id chid, struct asrtl_span buff )
{
        assert( r );
        assert( r->send_fn );
        return r->send_fn( r->send_data, chid, buff );
}

#endif
