#pragma once

#include "../asrtc/param.h"
#include "../asrtl/asrtl_assert.h"
#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"
#include "../asrtlpp/callback.hpp"
#include "../asrtlpp/flat_type_traits.hpp"
#include "../asrtlpp/sender.hpp"

namespace asrt
{

inline status init(
    ref< asrtc_param_server > srv,
    asrtl_node&               prev,
    autosender                sender,
    asrtl_allocator           alloc )
{
        return asrtc_param_server_init( srv, &prev, sender, alloc );
}

inline void set_tree( ref< asrtc_param_server > srv, asrtl_flat_tree const& tree )
{
        asrtc_param_server_set_tree( srv, &tree );
}

inline status send_ready(
    ref< asrtc_param_server >            srv,
    flat_id                              root_id,
    callback< asrtc_param_ready_ack_cb > ack_cb,
    uint32_t                             timeout )
{
        return asrtc_param_server_send_ready( srv, root_id, timeout, ack_cb.fn, ack_cb.ptr );
}

inline void deinit( ref< asrtc_param_server > srv )
{
        asrtc_param_server_deinit( srv );
}

}  // namespace asrt
