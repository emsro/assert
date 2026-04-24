#pragma once

#include "../asrtc/collect.h"
#include "../asrtl/asrtl_assert.h"
#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"
#include "../asrtlpp/callback.hpp"
#include "../asrtlpp/flat_type_traits.hpp"
#include "../asrtlpp/sender.hpp"
#include "../asrtlpp/util.hpp"

namespace asrt
{

inline status init(
    ref< asrtc_collect_server > srv,
    asrtl_node&                 prev,
    autosender                  send_cb,
    asrtl_allocator             alloc,
    uint32_t                    tree_block_cap,
    uint32_t                    tree_node_cap )
{
        return asrtc_collect_server_init(
            srv, &prev, send_cb, alloc, tree_block_cap, tree_node_cap );
}

inline status send_ready(
    ref< asrtc_collect_server >            srv,
    flat_id                                root_id,
    callback< asrtc_collect_ready_ack_cb > ack_cb,
    uint32_t                               timeout )
{
        return asrtc_collect_server_send_ready( srv, root_id, timeout, ack_cb.fn, ack_cb.ptr );
}

inline asrtl_flat_tree const& tree( ref< asrtc_collect_server > srv )
{
        return *asrtc_collect_server_tree( srv );
}

inline flat_id next_node_id( ref< asrtc_collect_server > srv )
{
        return srv->next_node_id;
}

inline void deinit( ref< asrtc_collect_server > srv )
{
        asrtc_collect_server_deinit( srv );
}

}  // namespace asrt
