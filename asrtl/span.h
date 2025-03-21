
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
#ifndef ASRTL_SPAN_H
#define ASRTL_SPAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct asrtl_span
{
        uint8_t* b;
        uint8_t* e;
};

static inline uint8_t asrtl_buffer_unfit( struct asrtl_span const* buff, uint32_t size )
{
        return ( buff->e - buff->b ) < (int32_t) size;
}

#ifdef __cplusplus
}
#endif

#endif
