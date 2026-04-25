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

typedef enum asrt_status ( *asrt_strm_msg_callback )( void* ptr, struct asrt_rec_span* buff );

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
static inline enum asrt_status asrt_msg_rtoc_strm_define(
    uint8_t                            schema_id,
    enum asrt_strm_field_type_e const* fields,
    uint16_t                           field_count,
    asrt_strm_msg_callback             cb,
    void*                              cb_ptr )
{
        if ( field_count > 255 )
                return ASRT_ARG_ERR;

        uint8_t hdr[3];
        hdr[0] = ASRT_STRM_MSG_DEFINE;
        hdr[1] = schema_id;
        hdr[2] = (uint8_t) field_count;

        uint8_t field_bytes[255];
        for ( uint16_t i = 0; i < field_count; i++ )
                field_bytes[i] = (uint8_t) fields[i];

        struct asrt_rec_span field_span = {
            .b = field_bytes, .e = field_bytes + field_count, .next = NULL };
        struct asrt_rec_span hdr_span = { .b = hdr, .e = hdr + sizeof hdr, .next = &field_span };
        return cb( cb_ptr, &hdr_span );
}

/// Send DATA message from reactor to controller.
/// Header: [MSG_DATA, schema_id] followed by raw record bytes.
static inline enum asrt_status asrt_msg_rtoc_strm_data(
    uint8_t                schema_id,
    uint8_t const*         data,
    uint16_t               data_size,
    asrt_strm_msg_callback cb,
    void*                  cb_ptr )
{
        uint8_t hdr[2];
        hdr[0] = ASRT_STRM_MSG_DATA;
        hdr[1] = schema_id;

        struct asrt_rec_span data_span = {
            .b = (uint8_t*) data, .e = (uint8_t*) data + data_size, .next = NULL };
        struct asrt_rec_span hdr_span = { .b = hdr, .e = hdr + sizeof hdr, .next = &data_span };
        return cb( cb_ptr, &hdr_span );
}

/// Send ERROR message from controller to reactor.
static inline enum asrt_status asrt_msg_ctor_strm_error(
    enum asrt_strm_err_e   error_code,
    asrt_strm_msg_callback cb,
    void*                  cb_ptr )
{
        uint8_t buf[2];
        buf[0]                    = ASRT_STRM_MSG_ERROR;
        buf[1]                    = error_code;
        struct asrt_rec_span span = { .b = buf, .e = buf + sizeof buf, .next = NULL };
        return cb( cb_ptr, &span );
}

#ifdef __cplusplus
}
#endif

#endif  // ASRT_STREAM_PROTO_H
