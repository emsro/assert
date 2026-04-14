#pragma once

#include "../asrtc/param.h"
#include "../asrtc/status_to_str.h"
#include "../asrtl/asrtl_assert.h"
#include "../asrtl/log.h"
#include "../asrtlpp/callback.hpp"
#include "../asrtlpp/flat_type_traits.hpp"
#include "../asrtlpp/sender.hpp"

namespace asrtc
{

struct param_server
{
        template < typename CB >
        param_server( asrtl_node* prev, CB& send_cb, struct asrtl_allocator alloc )
        {
                if ( auto s = asrtc_param_server_init(
                         &server_, prev, asrtl::make_sender( send_cb ), alloc );
                     s != ASRTC_SUCCESS ) {
                        ASRTL_ERR_LOG( "asrtc_param", "init failed: %s", asrtc_status_to_str( s ) );
                        ASRTL_ASSERT( false );
                }
        }

        param_server( param_server&& )      = delete;
        param_server( param_server const& ) = delete;

        asrtl_node* node()
        {
                return &server_.node;
        }

        void set_tree( asrtl_flat_tree const* tree )
        {
                asrtc_param_server_set_tree( &server_, tree );
        }

        [[nodiscard]] asrtl_status send_ready(
            asrtl::flat_id                              root_id,
            asrtl::callback< asrtc_param_ready_ack_cb > ack_cb,
            uint32_t                                    timeout )
        {
                return asrtc_param_server_send_ready(
                    &server_, root_id, timeout, ack_cb.fn, ack_cb.ptr );
        }

        ~param_server()
        {
                asrtc_param_server_deinit( &server_ );
        }

private:
        asrtc_param_server server_;
};

}  // namespace asrtc
