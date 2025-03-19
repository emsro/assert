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
#ifndef ASRTL_ECODE_TO_STR_H
#define ASRTL_ECODE_TO_STR_H

#include "./ecode.h"

static inline char* asrtl_ecode_to_str( enum asrtl_ecode ec )
{
        switch ( ec ) {
        case ASRTL_ASE1:
                return "ASE1: Failed to find test";
        case ASRTL_ASE2:
                return "ASE2: Test is already running";
        }
        return "ASE?: Unknown error code";
}

#endif
