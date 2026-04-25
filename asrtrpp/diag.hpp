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
#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"
#include "../asrtlpp/sender.hpp"
#include "../asrtr/diag.h"

#include <cstdint>

namespace asrt
{

void rec_diag(
    ref< asrt_diag_client > d,
    char const*             file,
    uint32_t                line,
    char const*             extra = nullptr )
{
        asrt_diag_client_record( d, file, line, extra );
}

inline status init( ref< asrt_diag_client > d, asrt_node& prev, autosender sender )
{
        return asrt_diag_client_init( d, &prev, sender );
}

inline void deinit( ref< asrt_diag_client > d )
{
        asrt_diag_client_deinit( d );
}

}  // namespace asrt
