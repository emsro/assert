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
#include "../asrtlpp/task.hpp"
#include "../asrtlpp/util.hpp"

namespace asrt
{

/// Initialise a collect server, linked after @p prev.  @p tree_block_cap and @p tree_node_cap
/// set the capacity of the internal flat_tree storage.
ASRT_NODISCARD inline status init(
    ref< asrt_collect_server > srv,
    asrt_node&                 prev,
    asrt_allocator             alloc,
    uint32_t                   tree_block_cap,
    uint32_t                   tree_node_cap )
{
        return asrt_collect_server_init( srv, &prev, alloc, tree_block_cap, tree_node_cap );
}

/// Send a READY message to the reactor to start a collection session rooted at @p root_id.
/// @p ack_cb fires once the reactor acknowledges or the operation times out.
ASRT_NODISCARD inline status send_ready(
    ref< asrt_collect_server >            srv,
    flat_id                               root_id,
    callback< asrt_collect_ready_ack_cb > ack_cb,
    uint32_t                              timeout )
{
        return asrt_collect_server_send_ready( srv, root_id, timeout, ack_cb.fn, ack_cb.ptr );
}

/// Access the flat_tree assembled from incoming APPEND messages.
inline asrt_flat_tree const& tree( ref< asrt_collect_server > srv )
{
        return *asrt_collect_server_tree( srv );
}

/// Return the next node ID the server will assign (useful for pre-allocating root IDs).
ASRT_NODISCARD inline flat_id next_node_id( ref< asrt_collect_server > srv )
{
        return srv->next_node_id;
}

/// Free the flat_tree storage and all server resources.
inline void deinit( ref< asrt_collect_server > srv )
{
        asrt_collect_server_deinit( srv );
}

/// Sender backing co_await send_ready(srv, root_id, timeout).
/// Completes with void once the reactor acknowledges the READY message.
struct collect_send_ready_sender
{
        using sender_concept = ecor::sender_t;
        using completion_signatures =
            ecor::completion_signatures< ecor::set_value_t(), ecor::set_error_t( status ) >;

        asrt_collect_server* srv;
        flat_id              root_id;
        uint32_t             timeout;

        template < ecor::receiver R >
        struct op
        {
                asrt_collect_server* srv;
                flat_id              root_id;
                uint32_t             timeout;
                R                    recv;

                void start()
                {
                        auto s = asrt_collect_server_send_ready(
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

/// co_await send_ready(srv, root_id, timeout) — advertise the tree to the reactor;
/// completes with void once the READY_ACK arrives.
inline ecor::sender auto send_ready(
    ref< asrt_collect_server > srv,
    flat_id                    root_id,
    uint32_t                   timeout )
{
        return collect_send_ready_sender{ srv, root_id, timeout };
}

}  // namespace asrt
