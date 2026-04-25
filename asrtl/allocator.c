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

#include "./allocator.h"

char* asrt_realloc_str( struct asrt_allocator* a, struct asrt_span* buff )
{
        uint8_t* p = buff->b;
        for ( ; p != buff->e && *p != 0; ++p )
                ;
        uint32_t i   = p - buff->b;
        uint32_t s   = p == buff->e ? i + 1 : i;
        char*    res = asrt_alloc( a, s );
        if ( !res )
                return NULL;
        memcpy( res, buff->b, i );
        res[s - 1] = '\0';
        buff->b    = p == buff->e ? p : p + 1;
        return res;
}
