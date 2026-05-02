/// Permission to use, copy, modify, and/or distribute this software for any
/// purpose with or without fee is hereby granted.
///
/// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
/// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
/// AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
/// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
/// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
/// OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
/// PERFORMANCE OF THIS SOFTWARE.
#ifndef ASRT_PROTO_H
#define ASRT_PROTO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../asrtl/chann.h"
#include "./status.h"
#include "util.h"

#include <stdint.h>

enum asrt_message_id_e
{
        ASRT_MSG_PROTO_VERSION = 0x01,  // reactor <-> controller
        ASRT_MSG_DESC          = 0x02,  // reactor <-> controller
        ASRT_MSG_TEST_COUNT    = 0x03,  // reactor <-> controller
        ASRT_MSG_TEST_INFO     = 0x04,  // reactor <-> controller
        ASRT_MSG_TEST_START    = 0x05,  // reactor <-> controller
        ASRT_MSG_TEST_RESULT   = 0x06,  // reactor -> controller
};

typedef uint16_t asrt_message_id;

static inline struct asrt_send_req* asrt_msg_u16( struct asrt_u8d2msg* msg, uint16_t val )
{
        uint8_t* p = msg->buff;
        asrt_add_u16( &p, val );
        msg->req.buff = ( struct asrt_span_span ){
            .b          = msg->buff,
            .e          = msg->buff + sizeof msg->buff,
            .rest       = NULL,
            .rest_count = 0,
        };
        return &msg->req;
}

static inline struct asrt_send_req* asrt_msg_rtoc_proto_version(
    struct asrt_u8d8msg* msg,
    uint16_t             major,
    uint16_t             minor,
    uint16_t             patch )
{
        uint8_t* p = msg->buff;
        asrt_add_u16( &p, ASRT_MSG_PROTO_VERSION );
        asrt_add_u16( &p, major );
        asrt_add_u16( &p, minor );
        asrt_add_u16( &p, patch );
        msg->req.buff = ( struct asrt_span_span ){
            .b          = msg->buff,
            .e          = msg->buff + sizeof msg->buff,
            .rest       = NULL,
            .rest_count = 0,
        };
        return &msg->req;
}
static inline struct asrt_send_req* asrt_msg_ctor_proto_version( struct asrt_u8d2msg* msg )
{
        return asrt_msg_u16( msg, ASRT_MSG_PROTO_VERSION );
}

struct asrt_core_desc_msg
{
        struct asrt_span     str_span;
        uint8_t              hdr[2];
        struct asrt_send_req req;
};

static inline struct asrt_send_req* asrt_msg_rtoc_desc(
    struct asrt_core_desc_msg* msg,
    char const*                desc,
    uint32_t                   desc_size )
{
        uint8_t* h = msg->hdr;
        asrt_add_u16( &h, ASRT_MSG_DESC );
        msg->str_span =
            ( struct asrt_span ){ .b = (uint8_t*) desc, .e = (uint8_t*) desc + desc_size };
        msg->req.buff = ( struct asrt_span_span ){
            .b          = msg->hdr,
            .e          = msg->hdr + sizeof msg->hdr,
            .rest       = &msg->str_span,
            .rest_count = 1,
        };
        return &msg->req;
}

static inline struct asrt_send_req* asrt_msg_ctor_desc( struct asrt_u8d2msg* msg )
{
        return asrt_msg_u16( msg, ASRT_MSG_DESC );
}

static inline struct asrt_send_req* asrt_msg_rtoc_test_count(
    struct asrt_u8d4msg* msg,
    uint16_t             count )
{
        uint8_t* p = msg->buff;
        asrt_add_u16( &p, ASRT_MSG_TEST_COUNT );
        asrt_add_u16( &p, count );

        msg->req.buff = ( struct asrt_span_span ){
            .b          = msg->buff,
            .e          = msg->buff + sizeof msg->buff,
            .rest       = NULL,
            .rest_count = 0,
        };
        return &msg->req;
}

static inline struct asrt_send_req* asrt_msg_ctor_test_count( struct asrt_u8d2msg* msg )
{
        return asrt_msg_u16( msg, ASRT_MSG_TEST_COUNT );
}

enum asrt_test_info_result_e
{
        ASRT_TEST_INFO_SUCCESS          = 0x01,
        ASRT_TEST_INFO_MISSING_TEST_ERR = 0x02,
};
typedef uint8_t asrt_test_info_result;

struct asrt_core_test_info_msg
{
        struct asrt_span     str_span;
        uint8_t              hdr[5];
        struct asrt_send_req req;
};

static inline struct asrt_send_req* asrt_msg_rtoc_test_info(
    struct asrt_core_test_info_msg* msg,
    uint16_t                        id,
    asrt_test_info_result           res,
    char const*                     desc,
    uint32_t                        desc_size )
{
        uint8_t* h = msg->hdr;
        asrt_add_u16( &h, ASRT_MSG_TEST_INFO );
        asrt_add_u16( &h, id );
        *h++ = (uint8_t) res;
        msg->str_span =
            ( struct asrt_span ){ .b = (uint8_t*) desc, .e = (uint8_t*) desc + desc_size };
        msg->req.buff = ( struct asrt_span_span ){
            .b          = msg->hdr,
            .e          = msg->hdr + sizeof msg->hdr,
            .rest       = &msg->str_span,
            .rest_count = 1,
        };
        return &msg->req;
}

static inline struct asrt_send_req* asrt_msg_ctor_test_info( struct asrt_u8d4msg* msg, uint16_t id )
{
        uint8_t* p = msg->buff;
        asrt_add_u16( &p, ASRT_MSG_TEST_INFO );
        asrt_add_u16( &p, id );
        msg->req.buff = ( struct asrt_span_span ){
            .b          = msg->buff,
            .e          = msg->buff + sizeof msg->buff,
            .rest       = NULL,
            .rest_count = 0,
        };
        return &msg->req;
}


static inline struct asrt_send_req* asrt_msg_rtoc_test_start(
    struct asrt_u8d8msg* msg,
    uint16_t             test_id,
    uint32_t             run_id )
{
        uint8_t* p = msg->buff;
        asrt_add_u16( &p, ASRT_MSG_TEST_START );
        asrt_add_u16( &p, test_id );
        asrt_add_u32( &p, run_id );
        msg->req.buff = ( struct asrt_span_span ){
            .b          = msg->buff,
            .e          = msg->buff + sizeof msg->buff,
            .rest       = NULL,
            .rest_count = 0,
        };
        return &msg->req;
}

static inline struct asrt_send_req* asrt_msg_ctor_test_start(
    struct asrt_u8d8msg* msg,
    uint16_t             test_id,
    uint32_t             run_id )
{
        uint8_t* p = msg->buff;
        asrt_add_u16( &p, ASRT_MSG_TEST_START );
        asrt_add_u16( &p, test_id );
        asrt_add_u32( &p, run_id );
        msg->req.buff = ( struct asrt_span_span ){
            .b          = msg->buff,
            .e          = msg->buff + sizeof msg->buff,
            .rest       = NULL,
            .rest_count = 0,
        };
        return &msg->req;
}

enum asrt_test_result_e
{
        ASRT_TEST_RESULT_SUCCESS = 0x01,  // Test executed successfully
        ASRT_TEST_RESULT_ERROR =
            0x02,  // Test execution resulted in an error (e.g. test code crashed, or
                   // test start was requested for non-existing test)
        ASRT_TEST_RESULT_FAILURE = 0x03,  // Test executed and did not pass (e.g. assertion failure)
};
typedef uint16_t asrt_test_result;

static inline struct asrt_send_req* asrt_msg_rtoc_test_result(
    struct asrt_u8d8msg* msg,
    uint32_t             run_id,
    asrt_test_result     res )
{
        uint8_t* p = msg->buff;
        asrt_add_u16( &p, ASRT_MSG_TEST_RESULT );
        asrt_add_u32( &p, run_id );
        asrt_add_u16( &p, res );
        msg->req.buff = ( struct asrt_span_span ){
            .b          = msg->buff,
            .e          = msg->buff + sizeof msg->buff,
            .rest       = NULL,
            .rest_count = 0,
        };
        return &msg->req;
}

#ifdef __cplusplus
}
#endif

#endif
