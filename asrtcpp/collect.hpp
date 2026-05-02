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

#include "../asrtc/collect.h"
#include "../asrtl/asrt_assert.h"
#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"
#include "../asrtlpp/callback.hpp"
#include "../asrtlpp/flat_type_traits.hpp"
#include "../asrtlpp/util.hpp"

namespace asrt
{

inline status init(
    ref< asrt_collect_server > srv,
    asrt_node&                 prev,
    asrt_allocator             alloc,
    uint32_t                   tree_block_cap,
    uint32_t                   tree_node_cap )
{
        return asrt_collect_server_init( srv, &prev, alloc, tree_block_cap, tree_node_cap );
}

inline status send_ready(
    ref< asrt_collect_server >            srv,
    flat_id                               root_id,
    callback< asrt_collect_ready_ack_cb > ack_cb,
    uint32_t                              timeout )
{
        return asrt_collect_server_send_ready( srv, root_id, timeout, ack_cb.fn, ack_cb.ptr );
}

inline asrt_flat_tree const& tree( ref< asrt_collect_server > srv )
{
        return *asrt_collect_server_tree( srv );
}

inline flat_id next_node_id( ref< asrt_collect_server > srv )
{
        return srv->next_node_id;
}

inline void deinit( ref< asrt_collect_server > srv )
{
        asrt_collect_server_deinit( srv );
}

}  // namespace asrt
