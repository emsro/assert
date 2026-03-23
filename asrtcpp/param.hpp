#pragma once

#include "../asrtc/param.h"
#include "../asrtlpp/sender.hpp"

namespace asrtc
{

struct param_server
{
        template < typename CB >
        param_server( asrtl_node* prev, CB& send_cb, struct asrtl_allocator alloc )
        {
                std::ignore =
                    asrtc_param_server_init( &server_, prev, asrtl::make_sender( send_cb ), alloc );
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

        [[nodiscard]] asrtl_status send_ready( asrtl_flat_id root_id )
        {
                return asrtc_param_server_send_ready( &server_, root_id );
        }

        asrtl_status tick()
        {
                return asrtc_param_server_tick( &server_ );
        }

        ~param_server()
        {
                asrtc_param_server_deinit( &server_ );
        }

private:
        asrtc_param_server server_;
};

}  // namespace asrtc
