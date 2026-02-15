#pragma once

#include "../asrtl/chann.h"
#include "../asrtl/cobs.h"
#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"
#include "../asrtr/status_to_str.h"
#include "../asrtrpp/reactor.hpp"
#include "./util.hpp"

#include <list>
#include <uv.h>

namespace asrtio
{

struct conn_ctx
{
        uv_tcp_t       client;
        bool           closed = false;
        asrtr::reactor reac;

        conn_ctx()
          : reac( *this, "simulator reactor" )
        {
                client.data = this;
        }

        asrtl::status operator()( asrtl::chann_id id, std::span< uint8_t > buff )
        {
                return rx.write( (uv_stream_t*) &client, id, buff );
        }

        void tick()
        {
                uint8_t buffer[256];
                auto    s = reac.tick( buffer );
                if ( s != ASRTR_SUCCESS ) {
                        ASRTL_ERR_LOG(
                            "test_rsim", "Reactor tick failed: %s", asrtr_status_to_str( s ) );
                }
        }

        cobs_node rx;


        void start()
        {
                rx.start( (uv_stream_t*) &client, reac.node(), [&]( ssize_t nread ) {
                        if ( nread < 0 )
                                close();
                } );
        }

        void close()
        {
                ASRTL_INF_LOG( "asrtio", "Closing rsim reactor connection" );
                if ( !closed ) {
                        uv_close( (uv_handle_t*) &client, []( uv_handle_t* h ) {
                                static_cast< conn_ctx* >( h->data )->closed = true;
                        } );
                }
        }
};

struct rsim_ctx
{
        uv_tcp_t              server;
        uv_idle_t             idle;
        std::list< conn_ctx > conns;

        uint16_t port()
        {
                struct sockaddr_storage bound_addr;
                int                     namelen = sizeof( bound_addr );
                int r = uv_tcp_getsockname( &server, (struct sockaddr*) &bound_addr, &namelen );
                if ( r != 0 ) {
                        ASRTL_ERR_LOG(
                            "asrtio", "Failed to get socket name: %s", uv_strerror( r ) );
                        return 0;
                }
                return ntohs( ( (struct sockaddr_in*) &bound_addr )->sin_port );
        }

        void tick()
        {
                std::erase_if( conns, []( conn_ctx const& c ) {
                        return c.closed;
                } );
                for ( auto& c : conns )
                        c.tick();
        }

        void start()
        {
                server.data = this;
                uv_idle_init( server.loop, &idle );
                idle.data = this;
                uv_idle_start( &idle, []( uv_idle_t* h ) {
                        static_cast< rsim_ctx* >( h->data )->tick();
                } );
                int r =
                    uv_listen( (uv_stream_t*) &server, 128, []( uv_stream_t* server, int status ) {
                            if ( status < 0 ) {
                                    ASRTL_ERR_LOG(
                                        "test_rsim", "Listen error: %s", uv_strerror( status ) );
                                    return;
                            }
                            auto& self = *static_cast< rsim_ctx* >( server->data );
                            auto& ctx  = self.conns.emplace_back();
                            uv_tcp_init( server->loop, &ctx.client );
                            if ( uv_accept( server, (uv_stream_t*) &ctx.client ) == 0 ) {
                                    ASRTL_INF_LOG( "test_rsim", "Accepted connection" );
                                    ctx.start();
                            } else {
                                    ctx.close();
                            }
                    } );
                if ( r != 0 )
                        ASRTL_ERR_LOG( "test_rsim", "uv_listen failed: %s", uv_strerror( r ) );
        }

        void close()
        {
                uv_idle_stop( &idle );
                uv_close( (uv_handle_t*) &idle, nullptr );
                for ( auto& c : conns )
                        c.close();
                uv_close( (uv_handle_t*) &server, nullptr );
        }
};

inline std::shared_ptr< asrtio::rsim_ctx > make_rsim( uv_loop_t* loop )
{
        std::shared_ptr< rsim_ctx > ctx = std::make_shared< rsim_ctx >();
        struct sockaddr_in          addr;
        uv_ip4_addr( "127.0.0.1", 0, &addr );

        int r = uv_tcp_init( loop, &ctx->server );
        if ( r != 0 ) {
                ASRTL_ERR_LOG( "asrtio", "Failed to initialize server: %s", uv_strerror( r ) );
                return nullptr;
        }

        r = uv_tcp_bind( &ctx->server, (const struct sockaddr*) &addr, 0 );
        if ( r != 0 ) {
                ASRTL_ERR_LOG( "asrtio", "Failed to bind to address: %s", uv_strerror( r ) );
                return nullptr;
        }

        return ctx;
}

}  // namespace asrtio
