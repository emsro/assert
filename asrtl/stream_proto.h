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
#ifndef ASRT_STREAM_PROTO_H
#define ASRT_STREAM_PROTO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "./chann.h"
#include "./status.h"
#include "./util.h"

/// Wire message IDs for the stream channel (ASRT_STRM = 6).
enum asrt_strm_message_id_e
{
        ASRT_STRM_MSG_DEFINE = 0x01,  // reactor    -> controller
        ASRT_STRM_MSG_DATA   = 0x02,  // reactor    -> controller
        ASRT_STRM_MSG_ERROR  = 0x03,  // controller -> reactor
};
typedef uint8_t asrt_strm_message_id;

/// Field type tags.
enum asrt_strm_field_type_e
{
        ASRT_STRM_FIELD_U8       = 0x01,
        ASRT_STRM_FIELD_U16      = 0x02,
        ASRT_STRM_FIELD_U32      = 0x03,
        ASRT_STRM_FIELD_I8       = 0x04,
        ASRT_STRM_FIELD_I16      = 0x05,
        ASRT_STRM_FIELD_I32      = 0x06,
        ASRT_STRM_FIELD_FLOAT    = 0x07,
        ASRT_STRM_FIELD_BOOL     = 0x08,
        ASRT_STRM_FIELD_LBRACKET = 0x09,  ///< Structural: open group (no wire data).
        ASRT_STRM_FIELD_RBRACKET = 0x0A,  ///< Structural: close group (no wire data).
};
typedef uint8_t asrt_strm_field_type;

/// Highest valid field type tag value.
#define ASRT_STRM_FIELD_TAG_MAX 0x0A

/// Error codes sent from controller to reactor.
enum asrt_strm_err_e
{
        ASRT_STRM_ERR_SUCCESS          = 0x00,
        ASRT_STRM_ERR_UNKNOWN_SCHEMA   = 0x01,
        ASRT_STRM_ERR_SIZE_MISMATCH    = 0x02,
        ASRT_STRM_ERR_ALLOC_FAILURE    = 0x03,
        ASRT_STRM_ERR_DUPLICATE_SCHEMA = 0x04,
        ASRT_STRM_ERR_INVALID_DEFINE   = 0x05,
};

/// Returns 1 if type_tag is a valid field type, 0 otherwise.
static inline uint8_t asrt_strm_field_valid( uint8_t type_tag )
{
        return type_tag >= 0x01 && type_tag <= ASRT_STRM_FIELD_TAG_MAX;
}

/// Returns the wire size in bytes for a given field type tag.
/// Structural tags (LBRACKET, RBRACKET) return 0.  Unknown tags return 0.
static inline uint8_t asrt_strm_field_size( asrt_strm_field_type type_tag )
{
        switch ( type_tag ) {
        case ASRT_STRM_FIELD_U8:
        case ASRT_STRM_FIELD_I8:
        case ASRT_STRM_FIELD_BOOL:
                return 1;
        case ASRT_STRM_FIELD_U16:
        case ASRT_STRM_FIELD_I16:
                return 2;
        case ASRT_STRM_FIELD_U32:
        case ASRT_STRM_FIELD_I32:
        case ASRT_STRM_FIELD_FLOAT:
                return 4;
        case ASRT_STRM_FIELD_LBRACKET:
        case ASRT_STRM_FIELD_RBRACKET:
        default:
                return 0;
        }
}

/// Return a human-readable label for a field type tag (e.g. "u32", "float").
static inline char const* asrt_strm_field_type_to_str( enum asrt_strm_field_type_e ft )
{
        switch ( ft ) {
        case ASRT_STRM_FIELD_U8:
                return "u8";
        case ASRT_STRM_FIELD_U16:
                return "u16";
        case ASRT_STRM_FIELD_U32:
                return "u32";
        case ASRT_STRM_FIELD_I8:
                return "i8";
        case ASRT_STRM_FIELD_I16:
                return "i16";
        case ASRT_STRM_FIELD_I32:
                return "i32";
        case ASRT_STRM_FIELD_FLOAT:
                return "float";
        case ASRT_STRM_FIELD_BOOL:
                return "bool";
        case ASRT_STRM_FIELD_LBRACKET:
                return "[";
        case ASRT_STRM_FIELD_RBRACKET:
                return "]";
        default:
                return "?";
        }
}

/// Send DEFINE message from reactor to controller.
/// Header: [MSG_DEFINE, schema_id, field_count] followed by field_count tags.
struct asrt_strm_define_msg
{
        struct asrt_span     field_span;
        uint8_t              fields[255];
        uint8_t              hdr[3];
        struct asrt_send_req req;
};

static inline struct asrt_send_req* asrt_msg_rtoc_strm_define(
    struct asrt_strm_define_msg*       msg,
    uint8_t                            schema_id,
    enum asrt_strm_field_type_e const* fields,
    uint16_t                           field_count )
{
        ASRT_ASSERT( field_count <= 255 );
        msg->hdr[0] = ASRT_STRM_MSG_DEFINE;
        msg->hdr[1] = schema_id;
        msg->hdr[2] = (uint8_t) field_count;
        for ( uint16_t i = 0; i < field_count; i++ )
                msg->fields[i] = (uint8_t) fields[i];
        msg->field_span = ( struct asrt_span ){ .b = msg->fields, .e = msg->fields + field_count };
        msg->req.buff   = ( struct asrt_span_span ){
              .b          = msg->hdr,
              .e          = msg->hdr + sizeof msg->hdr,
              .rest       = &msg->field_span,
              .rest_count = 1,
        };
        return &msg->req;
}

/// Send DATA message from reactor to controller.
/// Header: [MSG_DATA, schema_id] followed by raw record bytes.
struct asrt_strm_data_msg
{
        struct asrt_span     data_span;
        uint8_t              hdr[2];
        struct asrt_send_req req;
};

static inline struct asrt_send_req* asrt_msg_rtoc_strm_data(
    struct asrt_strm_data_msg* msg,
    uint8_t                    schema_id,
    uint8_t const*             data,
    uint16_t                   data_size )
{
        msg->hdr[0] = ASRT_STRM_MSG_DATA;
        msg->hdr[1] = schema_id;
        msg->data_span =
            ( struct asrt_span ){ .b = (uint8_t*) data, .e = (uint8_t*) data + data_size };
        msg->req.buff = ( struct asrt_span_span ){
            .b          = msg->hdr,
            .e          = msg->hdr + sizeof msg->hdr,
            .rest       = &msg->data_span,
            .rest_count = 1,
        };
        return &msg->req;
}

/// Send ERROR message from controller to reactor.
static inline struct asrt_send_req* asrt_msg_ctor_strm_error(
    struct asrt_u8d2msg* msg,
    enum asrt_strm_err_e error_code )
{
        msg->buff[0]  = ASRT_STRM_MSG_ERROR;
        msg->buff[1]  = error_code;
        msg->req.buff = ( struct asrt_span_span ){
            .b          = msg->buff,
            .e          = msg->buff + 2,
            .rest       = NULL,
            .rest_count = 0,
        };
        return &msg->req;
}

#ifdef __cplusplus
}
#endif

#endif  // ASRT_STREAM_PROTO_H
