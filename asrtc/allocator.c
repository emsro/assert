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

char* asrtc_realloc_str( struct asrtc_allocator* a, struct asrtl_span* buff )
{
        int      i = 0;
        int      n = 10000;  // XXX: random constant
        uint8_t* p = buff->b;
        for ( ;; ) {
                if ( p == buff->e )
                        break;
                if ( i++ == n )
                        return NULL;
                if ( *p == 0 )
                        break;
                p += 1;
        }
        uint32_t s   = p == buff->e ? i + 1 : i;
        char*    res = asrtc_alloc( a, s );
        memcpy( res, buff->b, i );
        res[s - 1] = '\0';
        buff->b    = p == buff->e ? p : p + 1;
        return res;
}
