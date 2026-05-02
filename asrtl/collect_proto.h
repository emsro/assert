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
#ifndef ASRT_COLLECT_PROTO_H
#define ASRT_COLLECT_PROTO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "./chann.h"
#include "./flat_tree.h"

/// Wire message IDs for the collect channel (ASRT_COLL = 5).
enum asrt_collect_message_id_e
{
        ASRT_COLLECT_MSG_READY     = 0x01,  // controller -> reactor
        ASRT_COLLECT_MSG_READY_ACK = 0x02,  // reactor    -> controller
        ASRT_COLLECT_MSG_APPEND    = 0x03,  // reactor    -> controller
        ASRT_COLLECT_MSG_ERROR     = 0x04,  // controller -> reactor
};
typedef uint8_t asrt_collect_message_id;

// node_id=0 is the reserved NONE sentinel.
#define ASRT_COLLECT_NONE_ID ( (asrt_flat_id) 0 )

enum asrt_collect_err_e
{
        ASRT_COLLECT_ERR_NONE          = 0x00,
        ASRT_COLLECT_ERR_APPEND_FAILED = 0x01,
};

typedef enum asrt_status ( *asrt_collect_msg_callback )( void* ptr, struct asrt_span* buff );

static inline struct asrt_send_req* asrt_msg_ctor_collect_ready(
    struct asrt_u8d9msg* ready_msg,
    asrt_flat_id         root_id,
    asrt_flat_id         next_node_id )
{
        uint8_t* p = ready_msg->buff;
        *p++       = ASRT_COLLECT_MSG_READY;
        asrt_add_u32( &p, root_id );
        asrt_add_u32( &p, next_node_id );
        ready_msg->req.buff = ( struct asrt_span_span ){
            .b          = ready_msg->buff,
            .e          = ready_msg->buff + sizeof ready_msg->buff,
            .rest       = NULL,
            .rest_count = 0,
        };
        return &ready_msg->req;
}

static inline struct asrt_send_req* asrt_msg_rtoc_collect_ready_ack(
    struct asrt_u8d1msg* ready_ack_msg )
{
        ready_ack_msg->buff[0]  = ASRT_COLLECT_MSG_READY_ACK;
        ready_ack_msg->req.buff = ( struct asrt_span_span ){
            .b          = ready_ack_msg->buff,
            .e          = ready_ack_msg->buff + 1,
            .rest       = NULL,
            .rest_count = 0,
        };
        return &ready_ack_msg->req;
}

struct asrt_collect_append_msg
{
        uint8_t              val_buff[9];
        struct asrt_span     span[3];
        uint8_t              hdr[9];
        struct asrt_send_req req;
};

static inline struct asrt_send_req* asrt_msg_rtoc_collect_append(
    struct asrt_collect_append_msg* msg,
    asrt_flat_id                    parent_id,
    asrt_flat_id                    node_id,
    char const*                     key,
    struct asrt_flat_value const*   value )
{
        // tail: type byte + encoded value (strings chain to their pointer)
        // OBJECT/ARRAY carry no value payload in the collect protocol — the
        // first/last child ids are managed by the receiving tree.
        uint8_t  val_buf[9];  // type(1) + max non-string payload(8)
        uint8_t* vp = val_buf;
        *vp++       = (uint8_t) value->type;

        struct asrt_span* key_span = &msg->span[0];
        struct asrt_span* val_span = &msg->span[1];

        uint32_t rest_count;
        if ( value->type == ASRT_FLAT_STYPE_STR ) {
                // span[1] = type byte, span[2] = string bytes (including NUL)
                size_t slen = strlen( value->data.s.str_val );
                memcpy( msg->val_buff, val_buf, (size_t) ( vp - val_buf ) );
                *val_span = ( struct asrt_span ){
                    .b = msg->val_buff, .e = msg->val_buff + (size_t) ( vp - val_buf ) };
                msg->span[2] = ( struct asrt_span ){
                    .b = (uint8_t*) value->data.s.str_val,
                    .e = (uint8_t*) value->data.s.str_val + slen + 1 };
                rest_count = 3;
        } else if ( value->type == ASRT_FLAT_CTYPE_OBJECT || value->type == ASRT_FLAT_CTYPE_ARRAY ) {
                memcpy( msg->val_buff, val_buf, (size_t) ( vp - val_buf ) );
                *val_span = ( struct asrt_span ){
                    .b = msg->val_buff, .e = msg->val_buff + (size_t) ( vp - val_buf ) };
                rest_count = 2;
        } else {
                asrt_flat_value_write( &vp, *value );
                memcpy( msg->val_buff, val_buf, (size_t) ( vp - val_buf ) );
                *val_span = ( struct asrt_span ){
                    .b = msg->val_buff, .e = msg->val_buff + (size_t) ( vp - val_buf ) };
                rest_count = 2;
        }

        // key: borrow caller's string (including NUL) or point at a static NUL byte
        if ( key ) {
                size_t klen = strlen( key );
                *key_span =
                    ( struct asrt_span ){ .b = (uint8_t*) key, .e = (uint8_t*) key + klen + 1 };
        } else {
                static uint8_t const nul = '\0';
                *key_span = ( struct asrt_span ){ .b = (uint8_t*) &nul, .e = (uint8_t*) &nul + 1 };
        }

        // header: msg_id + parent_id + node_id
        uint8_t* h = msg->hdr;
        *h++       = ASRT_COLLECT_MSG_APPEND;
        asrt_add_u32( &h, parent_id );
        asrt_add_u32( &h, node_id );
        msg->req.buff = ( struct asrt_span_span ){
            .b          = msg->hdr,
            .e          = msg->hdr + sizeof msg->hdr,
            .rest       = msg->span,
            .rest_count = rest_count,
        };
        return &msg->req;
}


static inline struct asrt_send_req* asrt_msg_ctor_collect_error(
    struct asrt_u8d2msg*    err_msg,
    enum asrt_collect_err_e error_code )
{
        err_msg->buff[0]  = ASRT_COLLECT_MSG_ERROR;
        err_msg->buff[1]  = (uint8_t) error_code;
        err_msg->req.buff = ( struct asrt_span_span ){
            .b          = err_msg->buff,
            .e          = err_msg->buff + sizeof err_msg->buff,
            .rest       = NULL,
            .rest_count = 0,
        };
        return &err_msg->req;
}

#ifdef __cplusplus
}
#endif

#endif  // ASRT_COLLECT_PROTO_H
