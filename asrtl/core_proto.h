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
        // XXX: stop running test
};

typedef uint16_t asrtl_message_id;

static inline enum asrtl_status asrtl_msg_rtoc_error(
    struct asrtl_span* buff,
    char const*        msg,
    uint32_t           msg_size )
{
        if ( asrtl_buffer_unfit( buff, sizeof( asrtl_message_id ) ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( &buff->b, ASRTL_MSG_ERROR );
        asrtl_fill_buffer( (uint8_t const*) msg, msg_size, buff );
        return ASRTL_SUCCESS;
}

static inline enum asrtl_status asrtl_msg_rtoc_proto_version(
    struct asrtl_span* buff,
    uint16_t           major,
    uint16_t           minor,
    uint16_t           patch )
{
        if ( asrtl_buffer_unfit( buff, sizeof( asrtl_message_id ) + 3 * sizeof( uint16_t ) ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( &buff->b, ASRTL_MSG_PROTO_VERSION );
        asrtl_add_u16( &buff->b, major );
        asrtl_add_u16( &buff->b, minor );
        asrtl_add_u16( &buff->b, patch );
        return ASRTL_SUCCESS;
}
static inline enum asrtl_status asrtl_msg_ctor_proto_version( struct asrtl_span* buff )
{
        if ( asrtl_buffer_unfit( buff, sizeof( asrtl_message_id ) ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( &buff->b, ASRTL_MSG_PROTO_VERSION );
        return ASRTL_SUCCESS;
}

static inline enum asrtl_status asrtl_msg_rtoc_desc(
    struct asrtl_span* buff,
    char const*        desc,
    uint32_t           desc_size )
{
        if ( asrtl_buffer_unfit( buff, sizeof( asrtl_message_id ) ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( &buff->b, ASRTL_MSG_DESC );
        asrtl_fill_buffer( (uint8_t const*) desc, desc_size, buff );
        return ASRTL_SUCCESS;
}

static inline enum asrtl_status asrtl_msg_ctor_desc( struct asrtl_span* buff )
{
        if ( asrtl_buffer_unfit( buff, sizeof( asrtl_message_id ) ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( &buff->b, ASRTL_MSG_DESC );
        return ASRTL_SUCCESS;
}

static inline enum asrtl_status asrtl_msg_rtoc_count( struct asrtl_span* buff, uint16_t count )
{
        if ( asrtl_buffer_unfit( buff, sizeof( asrtl_message_id ) + sizeof count ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( &buff->b, ASRTL_MSG_TEST_COUNT );
        asrtl_add_u16( &buff->b, count );
        return ASRTL_SUCCESS;
}

static inline enum asrtl_status asrtl_msg_ctor_test_count( struct asrtl_span* buff )
{
        if ( asrtl_buffer_unfit( buff, sizeof( asrtl_message_id ) ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( &buff->b, ASRTL_MSG_TEST_COUNT );
        return ASRTL_SUCCESS;
}

static inline enum asrtl_status asrtl_msg_rtoc_test_info(
    struct asrtl_span* buff,
    uint16_t           id,
    char const*        desc,
    uint32_t           desc_size )
{
        if ( asrtl_buffer_unfit( buff, sizeof( asrtl_message_id ) + sizeof( uint16_t ) ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( &buff->b, ASRTL_MSG_TEST_INFO );
        asrtl_add_u16( &buff->b, id );
        asrtl_fill_buffer( (uint8_t const*) desc, desc_size, buff );
        return ASRTL_SUCCESS;
}

static inline enum asrtl_status asrtl_msg_ctor_test_info( struct asrtl_span* buff, uint16_t id )
{
        if ( asrtl_buffer_unfit( buff, sizeof( asrtl_message_id ) + sizeof( uint16_t ) ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( &buff->b, ASRTL_MSG_TEST_INFO );
        asrtl_add_u16( &buff->b, id );
        return ASRTL_SUCCESS;
}

static inline enum asrtl_status asrtl_msg_ctor_test_start( struct asrtl_span* buff, uint16_t id )
{
        if ( asrtl_buffer_unfit( buff, sizeof( asrtl_message_id ) + sizeof( uint16_t ) ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( &buff->b, ASRTL_MSG_TEST_START );
        asrtl_add_u16( &buff->b, id );
        return ASRTL_SUCCESS;
}

static inline enum asrtl_status asrtl_msg_rtoc_test_start(
    struct asrtl_span* buff,
    uint16_t           test_id,
    uint32_t           run_id )
{
        if ( asrtl_buffer_unfit(
                 buff, sizeof( asrtl_message_id ) + sizeof( uint16_t ) + sizeof( uint32_t ) ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( &buff->b, ASRTL_MSG_TEST_START );
        asrtl_add_u16( &buff->b, test_id );
        asrtl_add_u32( &buff->b, run_id );
        return ASRTL_SUCCESS;
}

enum asrtl_test_result_e
{
        ASRTL_TEST_SUCCESS = 0x01,
        ASRTL_TEST_ERROR   = 0x02,
        ASRTL_TEST_FAILURE = 0x03,
};
typedef uint16_t asrtl_test_result;

static inline enum asrtl_status asrtl_msg_rtoc_test_result(
    struct asrtl_span* buff,
    uint32_t           run_id,
    asrtl_test_result  res,
    uint32_t           line )
{
        if ( asrtl_buffer_unfit(
                 buff,
                 sizeof( asrtl_message_id ) + sizeof( run_id ) + sizeof( asrtl_test_result ) +
                     sizeof( line ) ) )
                return ASRTL_SIZE_ERR;
        asrtl_add_u16( &buff->b, ASRTL_MSG_TEST_RESULT );
        asrtl_add_u32( &buff->b, run_id );
        asrtl_add_u16( &buff->b, res );
        asrtl_add_u32( &buff->b, line );
        return ASRTL_SUCCESS;
}

#endif
