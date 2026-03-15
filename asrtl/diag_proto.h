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
#ifndef ASRTL_DIAG_PROTO_H
#define ASRTL_DIAG_PROTO_H

#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "../asrtl/chann.h"
#include "../asrtl/status.h"
#include "../asrtl/util.h"

enum asrtl_diag_message_id_e
{
        ASRTL_DIAG_MSG_RECORD = 0x01,  // reactor -> controller
};

typedef uint8_t asrtl_diag_message_id;
typedef enum asrtl_status ( *asrtl_diag_msg_callback )( void* ptr, struct asrtl_rec_span* buff );

/// Sends a diag record message: 1-byte message ID, 4-byte line number, filename string.
static inline enum asrtl_status asrtl_msg_rtoc_diag_record(
    char const*             file,
    uint32_t                line,
    asrtl_diag_msg_callback cb,
    void*                   cb_ptr )
{
        uint8_t  prefix[5];
        uint8_t* p = prefix;
        *p++       = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, line );
        struct asrtl_rec_span prefix_buff = {
            .b = prefix, .e = prefix + sizeof prefix, .next = NULL };
        struct asrtl_rec_span file_buff = {
            .b = (uint8_t*) file, .e = (uint8_t*) file + strlen( file ), .next = NULL };
        prefix_buff.next = &file_buff;
        return cb( cb_ptr, &prefix_buff );
}

#ifdef __cplusplus
}
#endif

#endif  // ASRTL_DIAG_PROTO_H
