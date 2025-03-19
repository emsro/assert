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
#ifndef ASRTC_CALLBACKS_H
#define ASRTC_CALLBACKS_H

#include "./status.h"
#include "assert.h"
#include "stdint.h"

// XXX: might be asrtl thing?
enum asrtc_source
{
        ASRTC_CONTROLLER,
        ASRTC_REACTOR,
};
typedef enum asrtc_status (
    *asrtc_error_callback )( void* ptr, enum asrtc_source src, uint16_t ecode );
struct asrtc_error_cb
{
        void*                ptr;
        asrtc_error_callback cb;
};

static inline enum asrtc_status asrtc_raise_error(
    struct asrtc_error_cb* h,
    enum asrtc_source      src,
    uint16_t               ecode )
{
        assert( h && h->cb );
        return h->cb( h->ptr, src, ecode );
}


#endif
