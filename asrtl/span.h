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

#ifndef ASRT_SPAN_H
#define ASRT_SPAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/// Non-owning view of a contiguous byte range [b, e).
struct asrt_span
{
        uint8_t* b;  ///< Pointer to the first byte.
        uint8_t* e;  ///< One-past-the-end pointer.
};

/// Returns 1 if the buffer cannot fit @p size bytes, 0 otherwise.
static inline uint8_t asrt_span_unfit_for( struct asrt_span const* buff, uint32_t size )
{
        return (uint32_t) ( buff->e - buff->b ) < size ? 1U : 0U;
}

/// Scatter-gather buffer: a primary byte range [b, e) followed by @p rest_count additional spans.
/// Used to build multi-part messages without copying all segments into one buffer.
struct asrt_span_span
{
        uint8_t*          b;           ///< Primary buffer start.
        uint8_t*          e;           ///< Primary buffer one-past-the-end.
        struct asrt_span* rest;        ///< Optional array of additional byte ranges.
        uint32_t          rest_count;  ///< Number of entries in @p rest.
};

#ifdef __cplusplus
}
#endif

#endif
