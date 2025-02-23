
#ifndef ASRTR_REACTOR_H
#define ASRTR_REACTOR_H

#include "../asrtl/chann.h"
#include "../asrtl/core_proto.h"
#include "status.h"

#include <stdint.h>

enum asrtr_test_state
{
        ASRTR_TEST_INIT    = 1,
        ASRTR_TEST_RUNNING = 2,
        ASRTR_TEST_ERROR   = 3,
        ASRTR_TEST_FAIL    = 4,
        ASRTR_TEST_PASS    = 5,
};

struct asrtr_record;
typedef enum asrtr_status ( *asrtr_test_callback )( struct asrtr_record* );

struct asrtr_record
{
        enum asrtr_test_state state;
        void*                 data;
        asrtr_test_callback   continue_f;
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
        ASRTR_REC_IDLE        = 1,
        ASRTR_REC_TEST_EXEC   = 2,
        ASRTR_REC_TEST_REPORT = 3,
};

enum asrtr_reactor_flags
{
        ASRTR_FLAG_DESC      = 0x01,
        ASRTR_FLAG_PROTO_VER = 0x02,
        ASRTR_FLAG_TC        = 0x04,
        ASRTR_FLAG_TI        = 0x08,
        ASRTR_FLAG_TSTART    = 0x10,
};

struct asrtr_reactor
{
        struct asrtl_node    node;
        struct asrtl_sender* sendr;
        char const*          desc;

        struct asrtr_test* first_test;

        enum asrtr_reactor_state state;
        union
        {
                struct asrtr_record record;
        } state_data;

        uint32_t flags;  // values of asrtr_reactor_flags
        uint16_t recv_test_info_id;
        uint16_t recv_test_start_id;
};

enum asrtr_status
asrtr_reactor_init( struct asrtr_reactor* reac, struct asrtl_sender* sender, char const* desc );
enum asrtr_status
asrtr_reactor_tick( struct asrtr_reactor* reac, uint8_t* buffer, uint32_t buffer_size );

enum asrtr_status
asrtr_test_init( struct asrtr_test* t, char const* name, void* data, asrtr_test_callback start_f );
void asrtr_add_test( struct asrtr_reactor* reac, struct asrtr_test* test );

enum asrtl_status asrtr_reactor_recv( void* data, uint8_t const* msg, uint32_t msg_size );

#endif
