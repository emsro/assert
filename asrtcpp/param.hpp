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

#include "../asrtc/param.h"
#include "../asrtl/asrt_assert.h"
#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"
#include "../asrtlpp/callback.hpp"
#include "../asrtlpp/flat_type_traits.hpp"
#include "../asrtlpp/task.hpp"
#include "../asrtlpp/util.hpp"

namespace asrt
{

using status = enum asrt_status;

/// Initialise a param server and link it into the channel chain after @p prev.
ASRT_NODISCARD inline status init(
    ref< asrt_param_server > srv,
    asrt_node&               prev,
    asrt_allocator           alloc )
{
        return asrt_param_server_init( srv, &prev, alloc );
}

/// Set the flat_tree to be served in response to QUERY and FIND_BY_KEY requests.
/// The tree is borrowed — it must remain valid for the duration of the READY session.
inline void set_tree( ref< asrt_param_server > srv, asrt_flat_tree const& tree )
{
        asrt_param_server_set_tree( srv, &tree );
}

/// Advertise the param tree to the reactor with a READY message.
/// @p ack_cb fires once the reactor acknowledges or the operation times out.
ASRT_NODISCARD inline status send_ready(
    ref< asrt_param_server >            srv,
    flat_id                             root_id,
    callback< asrt_param_ready_ack_cb > ack_cb,
    uint32_t                            timeout )
{
        return asrt_param_server_send_ready( srv, root_id, timeout, ack_cb.fn, ack_cb.ptr );
}

/// Free all param server resources.
inline void deinit( ref< asrt_param_server > srv )
{
        asrt_param_server_deinit( srv );
}

/// Sender backing co_await send_ready(srv, root_id, timeout).
/// Completes with void once the reactor acknowledges the READY message.
struct param_send_ready_sender
{
        using sender_concept = ecor::sender_t;
        using completion_signatures =
            ecor::completion_signatures< ecor::set_value_t(), ecor::set_error_t( status ) >;

        asrt_param_server* srv;
        flat_id            root_id;
        uint32_t           timeout;

        template < ecor::receiver R >
        struct op
        {
                asrt_param_server* srv;
                flat_id            root_id;
                uint32_t           timeout;
                R                  recv;

                void start()
                {
                        auto s = asrt_param_server_send_ready(
                            srv,
                            root_id,
                            timeout,
                            +[]( void* p, enum asrt_status s ) {
                                    auto& self = *static_cast< op* >( p );
                                    if ( s == ASRT_SUCCESS )
                                            self.recv.set_value();
                                    else
                                            self.recv.set_error( s );
                            },
                            this );
                        if ( s != ASRT_SUCCESS )
                                recv.set_error( s );
                }
        };

        template < ecor::receiver R >
        auto connect( R&& r ) && noexcept
        {
                return op< R >{ srv, root_id, timeout, (R&&) r };
        }
};

/// co_await send_ready(srv, root_id, timeout) — advertise the param tree to the reactor;
/// completes with void once the READY_ACK arrives.
inline ecor::sender auto send_ready(
    ref< asrt_param_server > srv,
    flat_id                  root_id,
    uint32_t                 timeout )
{
        return param_send_ready_sender{ srv, root_id, timeout };
}

}  // namespace asrt
