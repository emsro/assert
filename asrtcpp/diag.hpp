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

#include "../asrtc/diag.h"
#include "../asrtl/asrt_assert.h"
#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"
#include "../asrtlpp/util.hpp"

#include <memory>

namespace asrt
{

using diag_record = asrt_diag_record;

struct diag_record_deleter
{
        struct asrt_allocator* alloc;
        void operator()( asrt_diag_record* rec ) const { asrt_diag_free_record( alloc, rec ); }
};

ASRT_NODISCARD inline status init(
    ref< asrt_diag_server > d,
    asrt_node&              prev,
    asrt_allocator          alloc )
{
        return asrt_diag_server_init( d, &prev, alloc );
}

ASRT_NODISCARD inline std::unique_ptr< diag_record, diag_record_deleter > take_record(
    ref< asrt_diag_server > d )
{
        return { asrt_diag_server_take_record( d ), diag_record_deleter{ &d->alloc } };
}

inline void deinit( ref< asrt_diag_server > d )
{
        asrt_diag_server_deinit( d );
}


}  // namespace asrt
