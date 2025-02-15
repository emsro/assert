
#ifndef ASRTR_REACTOR_H
#define ASRTR_REACTOR_H

#include "../asrtl/chann.h"
#include "status.h"

#include <stdint.h>

enum asrtr_state
{
        ASRTR_INIT    = 1,
        ASRTR_RUNNING = 2,
        ASRTR_ERROR   = 3,
        ASRTR_FAIL    = 4,
        ASRTR_PASS    = 5,
};

struct asrtr_record;
typedef enum asrtr_status ( *asrtr_test_callback )( struct asrtr_record* );

struct asrtr_record
{
        enum asrtr_state    state;
        void*               data;
        asrtr_test_callback continue_f;
};

struct asrtr_test
{
        char const*         name;
        void*               data;
        asrtr_test_callback start_f;
        struct asrtr_test*  next;
};

enum asrtr_reactor_state
{
        ASRTR_REC_IDLE = 1,
        ASRTR_REC_LIST = 2,
};

struct asrtr_reactor
{
        struct asrtl_node    node;
        struct asrtl_sender* sendr;
        struct asrtr_test*   first_test;

        enum asrtr_reactor_state state;
        union
        {
                struct asrtr_test* list_next;
        } state_data;
};

enum asrtr_status asrtr_reactor_init( struct asrtr_reactor* rec, struct asrtl_sender* sender );
enum asrtr_status asrtr_reactor_tick( struct asrtr_reactor* rec );

enum asrtr_status asrtr_reactor_list_event( struct asrtr_reactor* rec );

enum asrtr_status
asrtr_test_init( struct asrtr_test* t, char const* name, void* data, asrtr_test_callback start_f );
void asrtr_add_test( struct asrtr_reactor* rec, struct asrtr_test* test );

enum asrtl_status asrtr_reactor_recv( void* data, uint8_t const* msg, uint32_t msg_size );

#endif
