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

#include "../asrtl/asrt_assert.h"
#include "../asrtlpp/util.hpp"
#include "../asrtr/reac_assm.h"

namespace asrt
{

/// Initialise the reactor assembly — wires reactor, diag, param, collect and stream channels.
/// @p desc is a human-readable target description (borrowed).
/// @p param_timeout_ms is the maximum time to wait for a PARAM READY acknowledgement.
ASRT_NODISCARD inline enum asrt_status init(
    ref< asrt_reac_assm > assm,
    char const*           desc,
    uint32_t              param_timeout_ms )
{
        return asrt_reac_assm_init( assm, desc, param_timeout_ms );
}

/// Advance all assembly modules by one tick.
/// Must be called periodically from the main loop.
inline void tick( ref< asrt_reac_assm > assm, uint32_t now )
{
        asrt_reac_assm_tick( assm, now );
}

/// Append @p test to the assembly's reactor test list.
ASRT_NODISCARD inline enum asrt_status add_test( ref< asrt_reac_assm > assm, asrt_test& test )
{
        return asrt_reactor_add_test( &assm->reactor, &test );
}

/// Release all resources owned by the assembly.
inline void deinit( ref< asrt_reac_assm > assm )
{
        asrt_reac_assm_deinit( assm );
}

}  // namespace asrt
