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
        ASRTL_MSG_ERROR         = 0x01,  // reactor -> controller
        ASRTL_MSG_PROTO_VERSION = 0x02,  // reactor <-> controller
        ASRTL_MSG_DESC          = 0x03,  // reactor <-> controller
        ASRTL_MSG_TEST_COUNT    = 0x04,  // reactor <-> controller
        ASRTL_MSG_TEST_INFO     = 0x05,  // reactor <-> controller
        ASRTL_MSG_TEST_START    = 0x06,  // reactor <-> controller
        ASRTL_MSG_TEST_RESULT   = 0x07,  // reactor -> controller
};

typedef uint16_t asrtl_message_id;
typedef enum asrtl_status ( *asrtl_msg_callback )( void* ptr, struct asrtl_rec_span* buff );

static inline enum asrtl_status asrtl_msg_rtoc_error(
    uint16_t           ecode,
    asrtl_msg_callback cb,
    void*              cb_ptr )
{
        uint8_t  id[4];
        uint8_t* p = id;
        asrtl_add_u16( &p, ASRTL_MSG_ERROR );
        asrtl_add_u16( &p, ecode );
        struct asrtl_rec_span buff = { .b = id, .e = id + sizeof id, .next = NULL };
        return cb( cb_ptr, &buff );
}

static inline enum asrtl_status asrtl_msg_rtoc_proto_version(
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
static inline enum asrtl_status asrtl_msg_ctor_proto_version( struct asrtl_span* buff )
{
        if ( 0U != asrtl_span_unfit_for( buff, sizeof( asrtl_message_id ) ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( &buff->b, ASRTL_MSG_PROTO_VERSION );
        return ASRTL_SUCCESS;
}

static inline enum asrtl_status asrtl_msg_rtoc_desc(
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

static inline enum asrtl_status asrtl_msg_ctor_desc( struct asrtl_span* buff )
{
        if ( 0U != asrtl_span_unfit_for( buff, sizeof( asrtl_message_id ) ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( &buff->b, ASRTL_MSG_DESC );
        return ASRTL_SUCCESS;
}

static inline enum asrtl_status asrtl_msg_rtoc_test_count(
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

static inline enum asrtl_status asrtl_msg_ctor_test_count( struct asrtl_span* buff )
{
        if ( 0U != asrtl_span_unfit_for( buff, sizeof( asrtl_message_id ) ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( &buff->b, ASRTL_MSG_TEST_COUNT );
        return ASRTL_SUCCESS;
}

static inline enum asrtl_status asrtl_msg_rtoc_test_info(
    uint16_t           id,
    char const*        desc,
    uint32_t           desc_size,
    asrtl_msg_callback cb,
    void*              cb_ptr )
{
        uint8_t  prefix[4];
        uint8_t* p = prefix;
        asrtl_add_u16( &p, ASRTL_MSG_TEST_INFO );
        asrtl_add_u16( &p, id );
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

static inline enum asrtl_status asrtl_msg_ctor_test_info( struct asrtl_span* buff, uint16_t id )
{
        if ( 0U != asrtl_span_unfit_for( buff, sizeof( asrtl_message_id ) + sizeof( uint16_t ) ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( &buff->b, ASRTL_MSG_TEST_INFO );
        asrtl_add_u16( &buff->b, id );
        return ASRTL_SUCCESS;
}

static inline enum asrtl_status asrtl_msg_test_start(
    struct asrtl_span* buff,
    uint16_t           test_id,
    uint32_t           run_id )
{
        if ( 0U !=
             asrtl_span_unfit_for(
                 buff, sizeof( asrtl_message_id ) + sizeof( uint16_t ) + sizeof( uint32_t ) ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( &buff->b, ASRTL_MSG_TEST_START );
        asrtl_add_u16( &buff->b, test_id );
        asrtl_add_u32( &buff->b, run_id );
        return ASRTL_SUCCESS;
}

static inline enum asrtl_status asrtl_msg_rtoc_test_start(
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
    struct asrtl_span* buff,
    uint16_t           test_id,
    uint32_t           run_id )
{
        return asrtl_msg_test_start( buff, test_id, run_id );
}

enum asrtl_test_result_e
{
        ASRTL_TEST_SUCCESS = 0x01,
        ASRTL_TEST_ERROR   = 0x02,
        ASRTL_TEST_FAILURE = 0x03,
};
typedef uint16_t asrtl_test_result;

static inline enum asrtl_status asrtl_msg_rtoc_test_result(
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
