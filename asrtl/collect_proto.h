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
        ASRT_COLLECT_ERR_NONE = 0x00,
};

typedef enum asrt_status ( *asrt_collect_msg_callback )( void* ptr, struct asrt_rec_span* buff );

static inline enum asrt_status asrt_msg_ctor_collect_ready(
    asrt_flat_id              root_id,
    asrt_flat_id              next_node_id,
    asrt_collect_msg_callback cb,
    void*                     cb_ptr )
{
        uint8_t  buf[9];
        uint8_t* p = buf;
        *p++       = ASRT_COLLECT_MSG_READY;
        asrt_add_u32( &p, root_id );
        asrt_add_u32( &p, next_node_id );
        struct asrt_rec_span span = { .b = buf, .e = buf + sizeof buf, .next = NULL };
        return cb( cb_ptr, &span );
}

static inline enum asrt_status asrt_msg_rtoc_collect_ready_ack(
    asrt_collect_msg_callback cb,
    void*                     cb_ptr )
{
        uint8_t              buf[1] = { ASRT_COLLECT_MSG_READY_ACK };
        struct asrt_rec_span span   = { .b = buf, .e = buf + sizeof buf, .next = NULL };
        return cb( cb_ptr, &span );
}

static inline enum asrt_status asrt_msg_rtoc_collect_append(
    asrt_flat_id                  parent_id,
    asrt_flat_id                  node_id,
    char const*                   key,
    struct asrt_flat_value const* value,
    asrt_collect_msg_callback     cb,
    void*                         cb_ptr )
{
        // tail: type byte + encoded value (strings chain to their pointer)
        // OBJECT/ARRAY carry no value payload in the collect protocol — the
        // first/last child ids are managed by the receiving tree.
        uint8_t  val_buf[9];  // type(1) + max non-string payload(8)
        uint8_t* vp = val_buf;
        *vp++       = (uint8_t) value->type;

        struct asrt_rec_span str_span;
        struct asrt_rec_span val_span;
        if ( value->type == ASRT_FLAT_STYPE_STR ) {
                size_t slen = strlen( value->data.s.str_val );
                str_span    = ( struct asrt_rec_span ){
                       .b    = (uint8_t*) value->data.s.str_val,
                       .e    = (uint8_t*) value->data.s.str_val + slen + 1,
                       .next = NULL };
                val_span = ( struct asrt_rec_span ){ .b = val_buf, .e = vp, .next = &str_span };
        } else if ( value->type == ASRT_FLAT_CTYPE_OBJECT || value->type == ASRT_FLAT_CTYPE_ARRAY ) {
                val_span = ( struct asrt_rec_span ){ .b = val_buf, .e = vp, .next = NULL };
        } else {
                asrt_flat_value_write( &vp, *value );
                val_span = ( struct asrt_rec_span ){ .b = val_buf, .e = vp, .next = NULL };
        }

        // key: borrow caller's string (including NUL) or emit bare NUL
        uint8_t              nul = '\0';
        struct asrt_rec_span key_span;
        if ( key ) {
                size_t klen = strlen( key );
                key_span    = ( struct asrt_rec_span ){
                       .b = (uint8_t*) key, .e = (uint8_t*) key + klen + 1, .next = &val_span };
        } else {
                key_span = ( struct asrt_rec_span ){ .b = &nul, .e = &nul + 1, .next = &val_span };
        }

        // header: msg_id + parent_id + node_id
        uint8_t  hdr[9];
        uint8_t* h = hdr;
        *h++       = ASRT_COLLECT_MSG_APPEND;
        asrt_add_u32( &h, parent_id );
        asrt_add_u32( &h, node_id );
        struct asrt_rec_span head = { .b = hdr, .e = hdr + sizeof hdr, .next = &key_span };
        return cb( cb_ptr, &head );
}

static inline enum asrt_status asrt_msg_ctor_collect_error(
    enum asrt_collect_err_e   error_code,
    asrt_collect_msg_callback cb,
    void*                     cb_ptr )
{
        uint8_t  buf[2];
        uint8_t* p                = buf;
        *p++                      = ASRT_COLLECT_MSG_ERROR;
        *p++                      = (uint8_t) error_code;
        struct asrt_rec_span span = { .b = buf, .e = buf + sizeof buf, .next = NULL };
        return cb( cb_ptr, &span );
}

#ifdef __cplusplus
}
#endif

#endif  // ASRT_COLLECT_PROTO_H
