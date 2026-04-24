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

#ifdef __cplusplus
extern "C" {
#endif

#include "../asrtl/asrtl_assert.h"
#include "../asrtl/ecode.h"
#include "../asrtl/source.h"
#include "../asrtl/status.h"

#include <stdint.h>

typedef enum asrtl_status (
    *asrtc_error_callback )( void* ptr, enum asrtl_source src, enum asrtl_ecode ecode );
struct asrtc_error_cb
{
        void*                ptr;
        asrtc_error_callback cb;
};

static inline enum asrtl_status asrtc_raise_error(
    struct asrtc_error_cb* h,
    enum asrtl_source      src,
    enum asrtl_ecode       ecode )
{
        ASRTL_ASSERT( h && h->cb );
        return h->cb( h->ptr, src, ecode );
}

#ifdef __cplusplus
}
#endif

#endif
