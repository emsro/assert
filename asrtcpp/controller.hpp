
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
#pragma once

#include "../asrtc/controller.h"
#include "../asrtc/result.h"
#include "../asrtlpp/callback.hpp"
#include "../asrtlpp/sender.hpp"
#include "../asrtlpp/util.hpp"

#include <functional>

namespace asrt
{
using status = asrt_status;
using result = asrtc_result;

/// XXX: revise nodiscard in C++ code
/// XXX: revise status usage in C++ API - prefer sender
/// XXX: revisit inline usage and move stuff to .cpp

/// XXX: do a sender-based
inline status init( ref< asrtc_controller > c, autosender s, allocator a )
{
        return asrtc_cntr_init( c, s, a );
}

/// XXX: do a sender-based
inline status start(
    ref< asrtc_controller >         c,
    callback< asrtc_init_callback > cb,
    uint32_t                        timeout )
{
        return asrtc_cntr_start( c, cb.fn, cb.ptr, timeout );
}

inline bool is_idle( ref< asrtc_controller > c )
{
        return asrtc_cntr_idle( c ) > 0;
}

inline status query_desc(
    ref< asrtc_controller >         c,
    callback< asrtc_desc_callback > cb,
    uint32_t                        timeout )
{
        return asrtc_cntr_desc( c, cb.fn, cb.ptr, timeout );
}

inline status query_test_count(
    ref< asrtc_controller >               c,
    callback< asrtc_test_count_callback > cb,
    uint32_t                              timeout )
{
        return asrtc_cntr_test_count( c, cb.fn, cb.ptr, timeout );
}

inline status query_test_info(
    ref< asrtc_controller >              c,
    uint16_t                             id,
    callback< asrtc_test_info_callback > cb,
    uint32_t                             timeout )
{
        return asrtc_cntr_test_info( c, id, cb.fn, cb.ptr, timeout );
}

inline status exec_test(
    ref< asrtc_controller >                c,
    uint16_t                               id,
    callback< asrtc_test_result_callback > cb,
    uint32_t                               timeout )
{
        return asrtc_cntr_test_exec( c, id, cb.fn, cb.ptr, timeout );
}

inline void deinit( ref< asrtc_controller > c )
{
        asrtc_cntr_deinit( c );
}

}  // namespace asrt
