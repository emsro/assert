
#pragma once

#include "../asrtc/controller.h"
#include "../asrtc/result.h"
#include "../asrtlpp/callback.hpp"
#include "../asrtlpp/sender.hpp"
#include "../asrtlpp/util.hpp"

#include <functional>

namespace asrt
{
using status = asrtl_status;
using result = asrtc_result;

/// XXX: revise nodiscard in C++ code
/// XXX: revise status usage in C++ API - prefer sender
/// XXX: revisit inline usage and move stuff to .cpp

/// XXX: do a sender-based
inline status init(
    ref< asrtc_controller >          c,
    autosender                       s,
    allocator                        a,
    callback< asrtc_error_callback > ecb )
{
        return asrtc_cntr_init( c, s, a, { .ptr = ecb.ptr, .cb = ecb.fn } );
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
