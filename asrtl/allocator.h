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
#ifndef ASRTL_ALLOCATOR_H
#define ASRTL_ALLOCATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "./asrtl_assert.h"
#include "./span.h"

#include <stdlib.h>
#include <string.h>


struct asrtl_allocator
{
        void* ptr;
        void* ( *alloc )( void* ptr, uint32_t size );
        void ( *free )( void* ptr, void* mem );
};

static inline void* asrtl_alloc( struct asrtl_allocator* a, uint32_t size )
{
        ASRTL_ASSERT( a && a->free );
        return a->alloc( a->ptr, size );
}

static inline void asrtl_free( struct asrtl_allocator* a, void** mem )
{
        ASRTL_ASSERT( a && a->free );
        ASRTL_ASSERT( *mem );
        a->free( a->ptr, *mem );
        *mem = NULL;
}

char* asrtl_realloc_str( struct asrtl_allocator* a, struct asrtl_span* buff );

static inline void* asrtl_call_malloc( void* ptr, uint32_t size )
{
        (void) ptr;
        return malloc( size );
}

static inline void asrtl_call_free( void* ptr, void* mem )
{
        (void) ptr;
        free( mem );
}

static inline struct asrtl_allocator asrtl_default_allocator( void )
{
        return ( struct asrtl_allocator ){
            .ptr   = NULL,
            .alloc = &asrtl_call_malloc,
            .free  = &asrtl_call_free,
        };
}

#ifdef __cplusplus
}
#endif

#endif
