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
#ifndef ASRT_ASRT_ASSERT_H
#define ASRT_ASRT_ASSERT_H

/// Assertion hook.  Define ASRT_ASSERT before including this header to supply
/// a custom handler.  If ASRT_DEFAULT_ASSERT is defined, falls back to the
/// standard assert().  Otherwise silently compiles to a no-op so the library
/// can be used without any standard-library support.
#ifndef ASRT_ASSERT
#ifdef ASRT_DEFAULT_ASSERT
#include <assert.h>
#define ASRT_ASSERT( x ) assert( x )
#else
#define ASRT_ASSERT( x )                                                     \
        do {                                                                 \
                (void) sizeof( x ); /* NOLINT(bugprone-sizeof-expression) */ \
        } while ( 0 )
#endif
#endif

#endif
