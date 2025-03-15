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
#ifndef ASRTC_ALLOCATOR_H
#define ASRTC_ALLOCATOR_H

#include "../asrtl/util.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>


struct asrtc_allocator
{
        void* ptr;
        void* ( *alloc )( void* ptr, uint32_t size );
        void ( *free )( void* ptr, void* mem );
};

static inline void* asrtc_alloc( struct asrtc_allocator* a, uint32_t size )
{
        assert( a && a->free );
        return a->alloc( a->ptr, size );
}

static inline void asrtc_free( struct asrtc_allocator* a, void** mem )
{
        assert( a && a->free );
        assert( *mem );
        a->free( a->ptr, *mem );
        mem = NULL;
}

char* asrtc_realloc_str( struct asrtc_allocator* a, struct asrtl_span* buff );

#endif
