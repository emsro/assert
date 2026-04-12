#pragma once

#include "../asrtc/status_to_str.h"
#include "../asrtc/stream.h"
#include "../asrtl/asrtl_assert.h"
#include "../asrtl/log.h"
#include "../asrtlpp/sender.hpp"

namespace asrtc
{

struct stream_schemas
{
        stream_schemas() = default;

        explicit stream_schemas( asrtc_stream_schemas s )
          : s_( s )
        {
        }

        stream_schemas( stream_schemas const& )            = delete;
        stream_schemas& operator=( stream_schemas const& ) = delete;

        stream_schemas( stream_schemas&& o ) noexcept
          : s_( o.s_ )
        {
                o.s_ = {};
        }

        stream_schemas& operator=( stream_schemas&& o ) noexcept
        {
                if ( this != &o ) {
                        asrtc_stream_schemas_free( &s_ );
                        s_   = o.s_;
                        o.s_ = {};
                }
                return *this;
        }

        ~stream_schemas()
        {
                asrtc_stream_schemas_free( &s_ );
        }

        asrtc_stream_schemas const* operator->() const
        {
                return &s_;
        }

        asrtc_stream_schemas const& operator*() const
        {
                return s_;
        }

private:
        asrtc_stream_schemas s_{};
};

struct stream_server
{
        template < typename CB >
        stream_server(
            asrtl_node*            prev,
            CB&                    send_cb,
            struct asrtl_allocator alloc = asrtl_default_allocator() )
        {
                if ( auto s = asrtc_stream_server_init(
                         &server_, prev, asrtl::make_sender( send_cb ), alloc );
                     s != ASRTC_SUCCESS ) {
                        ASRTL_ERR_LOG(
                            "asrtc_stream", "init failed: %s", asrtc_status_to_str( s ) );
                        ASRTL_ASSERT( false );
                }
        }

        stream_server( stream_server&& )      = delete;
        stream_server( stream_server const& ) = delete;

        asrtl_node* node()
        {
                return &server_.node;
        }

        stream_schemas take()
        {
                return stream_schemas{ asrtc_stream_server_take( &server_ ) };
        }

        void clear()
        {
                asrtc_stream_server_clear( &server_ );
        }

        asrtc_status tick( uint32_t now )
        {
                return asrtc_stream_server_tick( &server_, now );
        }

        ~stream_server()
        {
                asrtc_stream_server_deinit( &server_ );
        }

private:
        asrtc_stream_server server_;
};

}  // namespace asrtc
