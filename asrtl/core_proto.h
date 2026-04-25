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
#ifndef ASRTL_PROTO_H
#define ASRTL_PROTO_H

#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "./status.h"
#include "util.h"

#include <stdint.h>

enum asrtl_message_id_e
{
        ASRTL_MSG_PROTO_VERSION = 0x01,  // reactor <-> controller
        ASRTL_MSG_DESC          = 0x02,  // reactor <-> controller
        ASRTL_MSG_TEST_COUNT    = 0x03,  // reactor <-> controller
        ASRTL_MSG_TEST_INFO     = 0x04,  // reactor <-> controller
        ASRTL_MSG_TEST_START    = 0x05,  // reactor <-> controller
        ASRTL_MSG_TEST_RESULT   = 0x06,  // reactor -> controller
};

typedef uint16_t asrtl_message_id;
typedef enum asrtl_status ( *asrtl_msg_callback )( void* ptr, struct asrtl_rec_span* buff );

static inline enum asrtl_status asrtl_msg_u16( uint16_t val, asrtl_msg_callback cb, void* cb_ptr )
{
        uint8_t  id[2];
        uint8_t* p = id;
        asrtl_add_u16( &p, val );
        struct asrtl_rec_span buff = { .b = id, .e = id + sizeof id, .next = NULL };
        return cb( cb_ptr, &buff );
}

static inline enum asrtl_status asrt_msg_rtoc_proto_version(
    uint16_t           major,
    uint16_t           minor,
    uint16_t           patch,
    asrtl_msg_callback cb,
    void*              cb_ptr )
{
        uint8_t  id[8];
        uint8_t* p = id;
        asrtl_add_u16( &p, ASRTL_MSG_PROTO_VERSION );
        asrtl_add_u16( &p, major );
        asrtl_add_u16( &p, minor );
        asrtl_add_u16( &p, patch );
        struct asrtl_rec_span buff = { .b = id, .e = id + sizeof id, .next = NULL };
        return cb( cb_ptr, &buff );
}
static inline enum asrtl_status asrtl_msg_ctor_proto_version( asrtl_msg_callback cb, void* cb_ptr )
{
        return asrtl_msg_u16( ASRTL_MSG_PROTO_VERSION, cb, cb_ptr );
}

static inline enum asrtl_status asrt_msg_rtoc_desc(
    char const*        desc,
    uint32_t           desc_size,
    asrtl_msg_callback cb,
    void*              cb_ptr )
{
        uint8_t id[2];
        asrtl_u16_to_u8d2( ASRTL_MSG_DESC, id );
        struct asrtl_rec_span id_buff   = { .b = id, .e = id + sizeof id, .next = NULL };
        struct asrtl_rec_span desc_buff = {
            .b = (uint8_t*) desc, .e = (uint8_t*) desc + desc_size, .next = NULL };
        id_buff.next = &desc_buff;
        return cb( cb_ptr, &id_buff );
}

static inline enum asrtl_status asrtl_msg_ctor_desc( asrtl_msg_callback cb, void* cb_ptr )
{
        return asrtl_msg_u16( ASRTL_MSG_DESC, cb, cb_ptr );
}

static inline enum asrtl_status asrt_msg_rtoc_test_count(
    uint16_t           count,
    asrtl_msg_callback cb,
    void*              cb_ptr )
{
        uint8_t  prefix[4];
        uint8_t* p = prefix;
        asrtl_add_u16( &p, ASRTL_MSG_TEST_COUNT );
        asrtl_add_u16( &p, count );

        struct asrtl_rec_span buff = { .b = prefix, .e = prefix + sizeof prefix, .next = NULL };
        return cb( cb_ptr, &buff );
}

static inline enum asrtl_status asrtl_msg_ctor_test_count( asrtl_msg_callback cb, void* cb_ptr )
{
        return asrtl_msg_u16( ASRTL_MSG_TEST_COUNT, cb, cb_ptr );
}

enum asrtl_test_info_result_e
{
        ASRTL_TEST_INFO_SUCCESS          = 0x01,
        ASRTL_TEST_INFO_MISSING_TEST_ERR = 0x02,
};
typedef uint8_t asrtl_test_info_result;

static inline enum asrtl_status asrt_msg_rtoc_test_info(
    uint16_t               id,
    asrtl_test_info_result res,
    char const*            desc,
    uint32_t               desc_size,
    asrtl_msg_callback     cb,
    void*                  cb_ptr )
{
        uint8_t  prefix[5];
        uint8_t* p = prefix;
        asrtl_add_u16( &p, ASRTL_MSG_TEST_INFO );
        asrtl_add_u16( &p, id );
        *p++                              = res;
        struct asrtl_rec_span prefix_buff = {
            .b = prefix, .e = prefix + sizeof prefix, .next = NULL };
        struct asrtl_rec_span desc_buff = {
            .b    = (uint8_t*) desc,
            .e    = (uint8_t*) desc + desc_size,
            .next = NULL,
        };
        prefix_buff.next = &desc_buff;
        return cb( cb_ptr, &prefix_buff );
}

static inline enum asrtl_status asrtl_msg_ctor_test_info(
    uint16_t           id,
    asrtl_msg_callback cb,
    void*              cb_ptr )
{
        uint8_t  prefix[4];
        uint8_t* p = prefix;
        asrtl_add_u16( &p, ASRTL_MSG_TEST_INFO );
        asrtl_add_u16( &p, id );
        struct asrtl_rec_span buff = { .b = prefix, .e = prefix + sizeof prefix, .next = NULL };
        return cb( cb_ptr, &buff );
}


static inline enum asrtl_status asrt_msg_rtoc_test_start(
    uint16_t           test_id,
    uint32_t           run_id,
    asrtl_msg_callback cb,
    void*              cb_ptr )
{
        uint8_t  id[8];
        uint8_t* p = id;
        asrtl_add_u16( &p, ASRTL_MSG_TEST_START );
        asrtl_add_u16( &p, test_id );
        asrtl_add_u32( &p, run_id );
        struct asrtl_rec_span buff = { .b = id, .e = id + sizeof id, .next = NULL };
        return cb( cb_ptr, &buff );
}

static inline enum asrtl_status asrtl_msg_ctor_test_start(
    uint16_t           test_id,
    uint32_t           run_id,
    asrtl_msg_callback cb,
    void*              cb_ptr )
{
        // The messages are the same, so we can reuse the rtoc version.
        return asrt_msg_rtoc_test_start( test_id, run_id, cb, cb_ptr );
}

enum asrtl_test_result_e
{
        ASRTL_TEST_SUCCESS = 0x01,  // Test executed successfully
        ASRTL_TEST_ERROR = 0x02,  // Test execution resulted in an error (e.g. test code crashed, or
                                  // test start was requested for non-existing test)
        ASRTL_TEST_FAILURE = 0x03,  // Test executed and did not pass (e.g. assertion failure)
};
typedef uint16_t asrtl_test_result;

static inline enum asrtl_status asrt_msg_rtoc_test_result(
    uint32_t           run_id,
    asrtl_test_result  res,
    asrtl_msg_callback cb,
    void*              cb_ptr )
{
        uint8_t  id[8];
        uint8_t* p = id;
        asrtl_add_u16( &p, ASRTL_MSG_TEST_RESULT );
        asrtl_add_u32( &p, run_id );
        asrtl_add_u16( &p, res );
        struct asrtl_rec_span buff = { .b = id, .e = id + sizeof id, .next = NULL };
        return cb( cb_ptr, &buff );
}

#ifdef __cplusplus
}
#endif

#endif
