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
#ifndef ASRT_DIAG_PROTO_H
#define ASRT_DIAG_PROTO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../asrtl/chann.h"
#include "../asrtl/status.h"
#include "../asrtl/util.h"

enum asrt_diag_message_id_e
{
        ASRT_DIAG_MSG_RECORD = 0x01,  // reactor -> controller
};

typedef uint8_t asrt_diag_message_id;

struct asrt_diag_record_msg
{
        struct asrt_span     spans[2];  // [0] = file, [1] = extra (optional)
        uint8_t              hdr[6];
        struct asrt_send_req req;
};

/// Builds a diag record message: 1-byte message ID, 4-byte line number, filename string.
static inline struct asrt_send_req* asrt_msg_rtoc_diag_record(
    struct asrt_diag_record_msg* msg,
    char const*                  file,
    uint32_t                     line,
    char const*                  extra )
{
        uint8_t* p = msg->hdr;
        *p++       = ASRT_DIAG_MSG_RECORD;
        asrt_add_u32( &p, line );
        size_t file_len = strlen( file );
        if ( file_len > UINT8_MAX ) {
                file     = "filename too long";
                file_len = strlen( file );
        }
        *p++ = (uint8_t) file_len;

        msg->spans[0] =
            ( struct asrt_span ){ .b = (uint8_t*) file, .e = (uint8_t*) file + file_len };
        uint32_t rest_count = 1;
        if ( extra ) {
                msg->spans[1] = ( struct asrt_span ){
                    .b = (uint8_t*) extra, .e = (uint8_t*) extra + strlen( extra ) };
                rest_count = 2;
        }
        msg->req.buff = ( struct asrt_span_span ){
            .b          = msg->hdr,
            .e          = msg->hdr + sizeof msg->hdr,
            .rest       = msg->spans,
            .rest_count = rest_count,
        };
        return &msg->req;
}

#ifdef __cplusplus
}
#endif

#endif  // ASRT_DIAG_PROTO_H
