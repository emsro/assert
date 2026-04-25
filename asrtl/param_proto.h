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
#ifndef ASRT_PARAM_PROTO_H
#define ASRT_PARAM_PROTO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "./chann.h"
#include "./flat_tree.h"
#include "./status.h"
#include "./util.h"

#include <string.h>

// Message IDs (uint8_t, channel 4)
enum asrt_param_message_id_e
{
        ASRT_PARAM_MSG_READY       = 0x01,  // controller -> reactor
        ASRT_PARAM_MSG_READY_ACK   = 0x02,  // reactor    -> controller
        ASRT_PARAM_MSG_QUERY       = 0x03,  // reactor    -> controller
        ASRT_PARAM_MSG_RESPONSE    = 0x04,  // controller -> reactor
        ASRT_PARAM_MSG_ERROR       = 0x05,  // controller -> reactor
        ASRT_PARAM_MSG_FIND_BY_KEY = 0x06,  // reactor    -> controller
};
typedef uint8_t asrt_param_message_id;

// node_id=0 is the reserved NONE sentinel.
#define ASRT_PARAM_NONE_ID ( (asrt_flat_id) 0 )

// Error codes in PARAM_ERROR payload
enum asrt_param_err_e
{
        ASRT_PARAM_ERR_NONE               = 0x00,
        ASRT_PARAM_ERR_RESPONSE_TOO_LARGE = 0x01,
        ASRT_PARAM_ERR_INVALID_QUERY      = 0x02,
        ASRT_PARAM_ERR_ENCODE_FAILURE     = 0x03,
        ASRT_PARAM_ERR_TYPE_MISMATCH      = 0x04,
        ASRT_PARAM_ERR_TIMEOUT            = 0x05,
};

typedef enum asrt_status ( *asrt_param_msg_callback )( void* ptr, struct asrt_rec_span* buff );


static inline enum asrt_status asrt_msg_ctor_param_ready(
    asrt_flat_id            root_id,
    asrt_param_msg_callback cb,
    void*                   cb_ptr )
{
        uint8_t  buf[5];
        uint8_t* p = buf;
        *p++       = ASRT_PARAM_MSG_READY;
        asrt_add_u32( &p, root_id );
        struct asrt_rec_span span = { .b = buf, .e = buf + sizeof buf, .next = NULL };
        return cb( cb_ptr, &span );
}

static inline enum asrt_status asrt_msg_rtoc_param_ready_ack(
    uint32_t                max_msg_size,
    asrt_param_msg_callback cb,
    void*                   cb_ptr )
{
        uint8_t  buf[5];
        uint8_t* p = buf;
        *p++       = ASRT_PARAM_MSG_READY_ACK;
        asrt_add_u32( &p, max_msg_size );
        struct asrt_rec_span span = { .b = buf, .e = buf + sizeof buf, .next = NULL };
        return cb( cb_ptr, &span );
}

static inline enum asrt_status asrt_msg_rtoc_param_query(
    asrt_flat_id            node_id,
    asrt_param_msg_callback cb,
    void*                   cb_ptr )
{
        uint8_t  buf[5];
        uint8_t* p = buf;
        *p++       = ASRT_PARAM_MSG_QUERY;
        asrt_add_u32( &p, node_id );
        struct asrt_rec_span span = { .b = buf, .e = buf + sizeof buf, .next = NULL };
        return cb( cb_ptr, &span );
}

static inline enum asrt_status asrt_msg_rtoc_param_find_by_key(
    struct asrt_span*       buff,
    asrt_flat_id            parent_id,
    char const*             key,
    asrt_param_msg_callback cb,
    void*                   cb_ptr )
{
        size_t   key_len = strlen( key );
        uint8_t* p       = buff->b;
        if ( 5U + key_len + 1U > (size_t) ( buff->e - buff->b ) )
                return ASRT_SIZE_ERR;
        *p++ = ASRT_PARAM_MSG_FIND_BY_KEY;
        asrt_add_u32( &p, parent_id );
        memcpy( p, key, key_len );
        p += key_len;
        *p++                      = '\0';
        struct asrt_rec_span span = { .b = buff->b, .e = p, .next = NULL };
        return cb( cb_ptr, &span );
}

static inline enum asrt_status asrt_msg_ctor_param_error(
    enum asrt_param_err_e   error_code,
    asrt_flat_id            node_id,
    asrt_param_msg_callback cb,
    void*                   cb_ptr )
{
        uint8_t  buf[6];
        uint8_t* p = buf;
        *p++       = ASRT_PARAM_MSG_ERROR;
        *p++       = error_code;
        asrt_add_u32( &p, node_id );
        struct asrt_rec_span span = { .b = buf, .e = buf + sizeof buf, .next = NULL };
        return cb( cb_ptr, &span );
}

// Returns the number of bytes the value payload occupies on the wire (not
// including node_id, key, or type byte).
static inline size_t asrt_param_value_wire_size( struct asrt_flat_value v )
{
        return asrt_flat_value_wire_size( v );
}

static inline void asrt_param_write_value( uint8_t** p, struct asrt_flat_value v )
{
        asrt_flat_value_write( p, v );
}

static inline enum asrt_status asrt_param_decode_value(
    struct asrt_span*       buff,
    uint8_t                 raw_type,
    struct asrt_flat_value* val )
{
        return asrt_flat_value_decode( buff, raw_type, val );
}

// Encodes msg_id + N sibling nodes (null-terminated keys/values) + trailing u32
// next_sibling_id into out_buff[0..max_bytes-1].  Writes total length to *out_len.
// Returns ASRT_SIZE_ERR when the first node does not fit, ASRT_ARG_ERR on bad args.
// A missing node is encoded as type=NONE with an empty key.
static inline enum asrt_status asrt_msg_ctor_param_response(
    struct asrt_flat_tree* tree,
    asrt_flat_id           start_id,
    uint32_t               max_bytes,
    uint8_t*               out_buff,
    uint32_t*              out_len )
{
        if ( !tree || !out_buff || !out_len )
                return ASRT_ARG_ERR;
        if ( max_bytes < 11U )  // msg_id(1)+node_id(4)+key\0(1)+type(1)+next_sib(4)
                return ASRT_SIZE_ERR;

        uint8_t*     p         = out_buff;
        uint8_t*     nodes_end = out_buff + max_bytes - 4;  // 4 bytes reserved for trailer
        asrt_flat_id current   = start_id;
        asrt_flat_id next_sib  = ASRT_PARAM_NONE_ID;
        int          encoded   = 0;

        *p++ = ASRT_PARAM_MSG_RESPONSE;

        while ( current != ASRT_PARAM_NONE_ID ) {
                struct asrt_flat_query_result qr;
                int not_found = ( asrt_flat_tree_query( tree, current, &qr ) != ASRT_SUCCESS );
                if ( not_found ) {
                        qr.id           = current;
                        qr.key          = NULL;
                        qr.value.type   = ASRT_FLAT_STYPE_NONE;
                        qr.next_sibling = ASRT_PARAM_NONE_ID;
                }

                size_t key_len   = qr.key ? strlen( qr.key ) : 0U;
                size_t node_size = 4U + key_len + 1U + 1U  // node_id + key\0 + type
                                   + asrt_param_value_wire_size( qr.value );

                if ( p + node_size > nodes_end ) {
                        if ( encoded == 0 )
                                return ASRT_SIZE_ERR;
                        next_sib = current;
                        break;
                }

                asrt_add_u32( &p, qr.id );
                if ( qr.key ) {
                        memcpy( p, qr.key, key_len );
                        p += key_len;
                }
                *p++ = '\0';
                *p++ = (uint8_t) qr.value.type;
                asrt_param_write_value( &p, qr.value );

                encoded++;
                if ( not_found )
                        break;
                current = qr.next_sibling;
        }

        asrt_add_u32( &p, next_sib );
        *out_len = (uint32_t) ( p - out_buff );
        return ASRT_SUCCESS;
}


#ifdef __cplusplus
}
#endif

#endif  // ASRT_PARAM_PROTO_H
