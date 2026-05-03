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
#ifndef ASRT_ALLOCATOR_H
#define ASRT_ALLOCATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "./asrt_assert.h"
#include "./span.h"

#include <stdlib.h>
#include <string.h>


/// Allocator vtable.  Pair a function pointer for allocation, one for freeing,
/// and an opaque context pointer that is forwarded to both.
struct asrt_allocator
{
        void* ptr;  ///< Opaque context passed to alloc/free.
        void* ( *alloc )( void* ptr, uint32_t size );
        void ( *free )( void* ptr, void* mem );
};

/// Allocate @p size bytes using the allocator.  Returns NULL on failure.
static inline void* asrt_alloc( struct asrt_allocator* a, uint32_t size )
{
        ASRT_ASSERT( a && a->free );
        return a->alloc( a->ptr, size );
}

/// Free memory previously returned by asrt_alloc and set *mem to NULL.
static inline void asrt_free( struct asrt_allocator* a, void** mem )
{
        ASRT_ASSERT( a && a->free );
        ASRT_ASSERT( *mem );
        a->free( a->ptr, *mem );
        *mem = NULL;
}

/// Copy the content of @p buff into a newly allocated NUL-terminated string.
/// Frees the previous content of @p buff->b first.  Returns the new pointer.
char* asrt_realloc_str( struct asrt_allocator* a, struct asrt_span* buff );

static inline void* asrt_call_malloc( void* ptr, uint32_t size )
{
        (void) ptr;  // context pointer unused by default malloc
        return malloc( size );
}

static inline void asrt_call_free( void* ptr, void* mem )
{
        (void) ptr;  // context pointer unused by default free
        free( mem );
}

/// Return an allocator backed by the process-level malloc/free.
static inline struct asrt_allocator asrt_default_allocator( void )
{
        return ( struct asrt_allocator ){
            .ptr   = NULL,
            .alloc = &asrt_call_malloc,
            .free  = &asrt_call_free,
        };
}

#ifdef __cplusplus
}
#endif

#endif
