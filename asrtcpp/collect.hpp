#pragma once

#include "../asrtc/collect.h"
#include "../asrtc/status_to_str.h"
#include "../asrtl/asrtl_assert.h"
#include "../asrtl/log.h"
#include "../asrtlpp/flat_type_traits.hpp"
#include "../asrtlpp/sender.hpp"
#include "../asrtlpp/util.hpp"

namespace asrtc
{

/// C++ wrapper for the controller-side collect server.
///
/// Owns an asrtc_collect_server and provides RAII lifetime, type-safe
/// send_ready() overloads, and const tree access.  See asrtc/collect.h
/// for protocol details.
struct collect_server
{
        /// Construct and link into the node chain.
        template < typename CB >
        collect_server(
            asrtl_node*            prev,
            CB&                    send_cb,
            struct asrtl_allocator alloc,
            uint32_t               tree_block_cap,
            uint32_t               tree_node_cap )
        {
                if ( auto s = asrtc_collect_server_init(
                         &server_,
                         prev,
                         asrtl::make_sender( send_cb ),
                         alloc,
                         tree_block_cap,
                         tree_node_cap );
                     s != ASRTC_SUCCESS ) {
                        ASRTL_ERR_LOG(
                            "asrtc_collect",
                            "init failed: %s",
                            asrtc_status_to_str( s ) );
                        ASRTL_ASSERT( false );
                }
        }

        collect_server( collect_server&& )      = delete;
        collect_server( collect_server const& ) = delete;

        asrtl_node* node()
        {
                return &server_.node;
        }

        /// Send READY with a lambda/functor ACK callback.
        template < typename CB >
        [[nodiscard]] asrtl_status
        send_ready( asrtl::flat_id root_id, CB& ack_cb, uint32_t timeout )
        {
                return asrtc_collect_server_send_ready(
                    &server_,
                    root_id,
                    timeout,
                    []( void* p, asrtc_status ) {
                            ( *reinterpret_cast< CB* >( p ) )();
                    },
                    &ack_cb );
        }

        /// Send READY with a C-style function-pointer ACK callback.
        [[nodiscard]] asrtl_status send_ready(
            asrtl::flat_id             root_id,
            asrtc_collect_ready_ack_cb ack_cb,
            void*                      ack_cb_ptr,
            uint32_t                   timeout )
        {
                return asrtc_collect_server_send_ready(
                    &server_, root_id, timeout, ack_cb, ack_cb_ptr );
        }

        asrtl_status tick( uint32_t now )
        {
                return asrtc_collect_server_tick( &server_, now );
        }

        /// Access the built tree.  Valid until next send_ready() or destruction.
        asrtl_flat_tree const* tree() const
        {
                return asrtc_collect_server_tree( &server_ );
        }

        /// Next auto-assigned node ID (matches the reactor's counter).
        asrtl::flat_id next_node_id() const
        {
                return server_.next_node_id;
        }

        ~collect_server()
        {
                asrtc_collect_server_deinit( &server_ );
        }

private:
        asrtc_collect_server server_;
};

}  // namespace asrtc
