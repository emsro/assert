
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
#ifndef ASRTC_TEST_RESULT_TO_STR_H
#define ASRTC_TEST_RESULT_TO_STR_H

#include "./result.h"

inline char const* asrtc_test_result_to_str( asrt_test_result res )
{
        switch ( res ) {
        case ASRT_TEST_RESULT_SUCCESS:
                return "success";
        case ASRT_TEST_RESULT_ERROR:
                return "error";
        case ASRT_TEST_RESULT_FAILURE:
                return "failure";
        default:
                break;
        }
        return "unknown result";
}

#endif
