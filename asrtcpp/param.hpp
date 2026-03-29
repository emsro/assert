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

        template < typename CB >
        [[nodiscard]] asrtl_status send_ready( asrtl_flat_id root_id, CB& ack_cb, uint32_t timeout )
        {
                return asrtc_param_server_send_ready(
                    &server_,
                    root_id,
                    timeout,
                    []( void* p, asrtc_status ) {
                            ( *reinterpret_cast< CB* >( p ) )();
                    },
                    &ack_cb );
        }

        asrtl_status tick( uint32_t now )
        {
                return asrtc_param_server_tick( &server_, now );
        }

        ~param_server()
        {
                asrtc_param_server_deinit( &server_ );
        }

private:
        asrtc_param_server server_;
};

}  // namespace asrtc
