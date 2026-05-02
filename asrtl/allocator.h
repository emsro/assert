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


struct asrt_allocator
{
        void* ptr;
        void* ( *alloc )( void* ptr, uint32_t size );
        void ( *free )( void* ptr, void* mem );
};

static inline void* asrt_alloc( struct asrt_allocator* a, uint32_t size )
{
        ASRT_ASSERT( a && a->free );
        return a->alloc( a->ptr, size );
}

static inline void asrt_free( struct asrt_allocator* a, void** mem )
{
        ASRT_ASSERT( a && a->free );
        ASRT_ASSERT( *mem );
        a->free( a->ptr, *mem );
        *mem = NULL;
}

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
