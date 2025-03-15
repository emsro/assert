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
#ifndef ASRTC_DEFAULT_ALLOCATOR_H
#define ASRTC_DEFAULT_ALLOCATOR_H

#include "./allocator.h"

static inline void* asrtc_call_malloc( void* ptr, uint32_t size )
{
        (void) ptr;
        return malloc( size );
}
static inline void asrtc_call_free( void* ptr, void* mem )
{
        (void) ptr;
        free( mem );
}

static inline struct asrtc_allocator default_allocator()
{
        return ( struct asrtc_allocator ){
            .ptr   = NULL,
            .alloc = &asrtc_call_malloc,
            .free  = &asrtc_call_free,
        };
}

#endif
