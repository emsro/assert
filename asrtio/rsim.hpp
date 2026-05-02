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

#include "../asrtl/chann.h"
#include "../asrtl/cobs.h"
#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"
#include "../asrtr/reac_assm.h"
#include "../asrtrpp/collect.hpp"
#include "../asrtrpp/diag.hpp"
#include "../asrtrpp/param.hpp"
#include "../asrtrpp/reactor.hpp"
#include "../asrtrpp/stream.hpp"
#include "./demo.hpp"
#include "./euv.hpp"
#include "./task.hpp"
#include "./util.hpp"

#include <list>
#include <random>
#include <uv.h>

namespace asrtio
{


struct conn_ctx
{
        uv_tcp_t                                    client;
        bool                                        disconnected = false;
        std::mt19937                                rng;
        asrt_reac_assm                              assm;
        std::list< asrt::unit< demo_test > >        demo_tests;
        std::vector< std::shared_ptr< asrt_test > > task_demo_tests;

        asrt::malloc_free_memory_resource mem;
        asrt::task_ctx                    task_ctx{ mem };

        conn_ctx( uint32_t seed )
          : rng( seed )
        {
                if ( asrt_reac_assm_init( &assm, "rsim", 100 ) != ASRT_SUCCESS ) {
                        ASRT_ERR_LOG( "asrtio", "Failed to initialize assembly" );
                        throw std::runtime_error( "Failed to initialize assembly" );
                }

                client.data = this;

                reg_demo( make_demo_pass() );
                reg_demo( make_demo_fail() );
                reg_demo( make_demo_check() );
                reg_demo( make_demo_check_fail() );
                reg_demo( make_demo_require_fail() );
                reg_demo( make_demo_counter() );
                reg_demo( make_demo_random() );
                reg_demo( make_demo_random_counter() );
                reg_demo( make_demo_param_value() );
                reg_demo( make_demo_param_count() );
                reg_demo( make_demo_param_find() );
                reg_task_demo< pass_demo_task >();
                reg_task_demo< fail_demo_task >();
                reg_task_demo< error_demo_task >();
                reg_task_demo< counter_demo_task >();
                reg_task_demo< check_demo_task >();
                reg_task_demo< check_fail_demo_task >();
                reg_task_demo< multi_step_fail_demo_task >();
                reg_task_demo< param_query_demo_task >( assm.param );
                reg_task_demo< param_type_overview_task >( assm.param );
                reg_task_demo< collect_demo_task >( assm.collect );
                reg_task_demo< stream_demo_task >( assm.stream );
                reg_task_demo< stream_sensor_demo_task >( assm.stream );
        }

        void reg_demo( demo_spec spec )
        {
                auto& t = demo_tests.emplace_back( std::move( spec ), assm.diag, assm.param, rng );
                if ( add_test( assm.reactor, t ) != ASRT_SUCCESS )
                        throw std::runtime_error( "add_test failed" );
        }

        template < typename T, typename... Args >
        void reg_task_demo( Args&&... args )
        {
                auto s =
                    std::make_shared< asrt::task_unit< T > >( T{ task_ctx, (Args&&) args... } );
                auto& t = task_demo_tests.emplace_back( s, (asrt_test*) s.get() );
                if ( asrt::add_test( assm.reactor, *t ) != ASRT_SUCCESS )
                        throw std::runtime_error( "add_test failed" );
        }

        void tick()
        {
                task_ctx.tick();
                auto now = uv_now( client.loop );
                asrt_reac_assm_tick( &assm, now );

                while ( auto* req = asrt_send_req_list_next( &assm.send_queue ) ) {
                        if ( disconnected ) {
                                asrt_send_req_list_done( &assm.send_queue, ASRT_SEND_ERR );
                                continue;
                        }
                        auto st = rx.write( (uv_stream_t*) &client, req->chid, req->buff );
                        asrt_send_req_list_done( &assm.send_queue, st );
                }
        }

        cobs_node rx;


        void start()
        {
                rx.start(
                    (uv_stream_t*) &client,
                    &assm.reactor.node,
                    "asrtio_rsim",
                    [&]( ssize_t nread ) {
                            if ( nread == UV_EOF ) {
                                    ASRT_DBG_LOG( "test_rsim", "Connection closed by remote" );
                            } else {
                                    ASRT_ERR_LOG(
                                        "test_rsim",
                                        "Read error: %s",
                                        uv_strerror( static_cast< int >( nread ) ) );
                            }
                            disconnect();
                    } );
        }

        void disconnect()
        {
                if ( !disconnected ) {
                        disconnected = true;
                        ASRT_INF_LOG( "asrtio", "Closing rsim reactor connection" );
                        uv_close( (uv_handle_t*) &client, nullptr );
                }
        }
};

inline task< void > async_close( task_ctx&, conn_ctx& ctx )
{
        if ( ctx.disconnected )
                co_return;
        ctx.disconnected = true;
        ASRT_INF_LOG( "asrtio", "Asynchronously closing rsim reactor connection" );
        co_await uv_close_handle{ (uv_handle_t*) &ctx.client };
}

struct rsim_ctx
{
        uv_loop_t*            loop;
        uv_tcp_t              server;
        uv_idle_t             idle;
        uint32_t              seed   = 0;
        bool                  closed = false;
        std::list< conn_ctx > conns;

        rsim_ctx( uv_loop_t* loop, uint32_t seed = 0 )
          : loop( loop )
          , seed( seed )
        {
        }

        uint16_t port()
        {
                struct sockaddr_storage bound_addr;
                int                     namelen = sizeof( bound_addr );
                int r = uv_tcp_getsockname( &server, (struct sockaddr*) &bound_addr, &namelen );
                if ( r != 0 ) {
                        ASRT_ERR_LOG( "asrtio", "Failed to get socket name: %s", uv_strerror( r ) );
                        return 0;
                }
                return ntohs( ( (struct sockaddr_in*) &bound_addr )->sin_port );
        }

        void tick()
        {
                std::erase_if( conns, []( conn_ctx const& c ) {
                        return c.disconnected;
                } );
                for ( auto& c : conns )
                        c.tick();
        }

        asrt::status start()
        {
                struct sockaddr_in addr;
                uv_ip4_addr( "127.0.0.1", 0, &addr );

                int r = uv_tcp_init( loop, &server );
                if ( r != 0 ) {
                        ASRT_ERR_LOG(
                            "asrtio", "Failed to initialize server: %s", uv_strerror( r ) );
                        return ASRT_INIT_ERR;
                }

                r = uv_tcp_bind( &server, (const struct sockaddr*) &addr, 0 );
                if ( r != 0 ) {
                        ASRT_ERR_LOG( "asrtio", "Failed to bind to address: %s", uv_strerror( r ) );
                        return ASRT_INIT_ERR;
                }
                server.data = this;
                uv_idle_init( server.loop, &idle );
                idle.data = this;
                uv_idle_start( &idle, []( uv_idle_t* h ) {
                        static_cast< rsim_ctx* >( h->data )->tick();
                } );
                r = uv_listen( (uv_stream_t*) &server, 128, []( uv_stream_t* server, int status ) {
                        if ( status < 0 ) {
                                ASRT_ERR_LOG(
                                    "test_rsim", "Listen error: %s", uv_strerror( status ) );
                                return;
                        }
                        auto& self = *static_cast< rsim_ctx* >( server->data );
                        auto& ctx  = self.conns.emplace_back( self.seed );
                        uv_tcp_init( server->loop, &ctx.client );
                        if ( uv_accept( server, (uv_stream_t*) &ctx.client ) == 0 ) {
                                ASRT_INF_LOG( "test_rsim", "Accepted connection" );
                                ctx.start();
                        } else {
                                ctx.disconnect();
                        }
                } );
                if ( r != 0 ) {
                        ASRT_ERR_LOG( "test_rsim", "uv_listen failed: %s", uv_strerror( r ) );
                        return ASRT_INIT_ERR;
                }
                return ASRT_SUCCESS;
        }

        void close()
        {
                if ( closed )
                        return;
                closed = true;
                uv_idle_stop( &idle );
                uv_close( (uv_handle_t*) &idle, nullptr );
                for ( auto& c : conns )
                        c.disconnect();
                uv_close( (uv_handle_t*) &server, nullptr );
        }

        friend task< void > async_destroy( task_ctx&, rsim_ctx& );
};

inline task< void > async_destroy( task_ctx& ctx, rsim_ctx& rs )
{
        if ( rs.closed )
                co_return;
        rs.closed = true;
        uv_idle_stop( &rs.idle );
        co_await uv_close_handle{ (uv_handle_t*) &rs.idle };
        for ( auto& c : rs.conns )
                co_await async_close( ctx, c );
        co_await uv_close_handle{ (uv_handle_t*) &rs.server };
}

}  // namespace asrtio
