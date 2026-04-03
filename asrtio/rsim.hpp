#pragma once

#include "../asrtl/chann.h"
#include "../asrtl/cobs.h"
#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"
#include "../asrtr/status_to_str.h"
#include "../asrtrpp/diag.hpp"
#include "../asrtrpp/param.hpp"
#include "../asrtrpp/reactor.hpp"
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
        uv_tcp_t                                     client;
        bool                                         closed = false;
        std::mt19937                                 rng;
        asrtr::reactor                               reac;
        asrtr::diag                                  r_diag;
        uint8_t                                      param_buf[256] = {};
        asrtr::param_client                          r_param;
        std::list< asrtr::unit< demo_test > >        demo_tests;
        std::vector< std::shared_ptr< asrtr_test > > task_demo_tests;

        asrtl::malloc_free_memory_resource mem;
        asrtr::task_ctx                    task_ctx{ mem };

        conn_ctx( uint32_t seed )
          : rng( seed )
          , reac( *this, "simulator reactor" )
          , r_diag( reac.node(), *this )
          , r_param(
                r_diag.node(),
                *this,
                asrtl_span{ .b = param_buf, .e = param_buf + sizeof( param_buf ) },
                100 )
        {
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
                reg_task_demo< param_query_demo_task >( r_param );
        }

        void reg_demo( demo_spec spec )
        {
                auto& t = demo_tests.emplace_back( std::move( spec ), r_diag, r_param, rng );
                reac.add_test( t );
        }

        template < typename T, typename... Args >
        void reg_task_demo( Args&&... args )
        {
                auto s =
                    std::make_shared< asrtr::task_unit< T > >( T{ task_ctx, (Args&&) args... } );
                auto& t = task_demo_tests.emplace_back( s, (asrtr_test*) s.get() );
                reac.add_test( *t );
        }

        asrtr::param_client& param()
        {
                return r_param;
        }

        asrtl::status operator()( asrtl::chann_id id, asrtl::rec_span* buff )
        {
                return rx.write( (uv_stream_t*) &client, id, *buff );
        }

        void tick()
        {
                task_ctx.tick();
                auto s = reac.tick();
                if ( s != ASRTR_SUCCESS ) {
                        ASRTL_ERR_LOG(
                            "test_rsim", "Reactor tick failed: %s", asrtr_status_to_str( s ) );
                }
                std::ignore = r_param.tick();
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
                if ( !closed ) {
                        closed = true;
                        ASRTL_INF_LOG( "asrtio", "Closing rsim reactor connection" );
                        uv_close( (uv_handle_t*) &client, nullptr );
                }
        }
};

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

        status start()
        {
                struct sockaddr_in addr;
                uv_ip4_addr( "127.0.0.1", 0, &addr );

                int r = uv_tcp_init( loop, &server );
                if ( r != 0 ) {
                        ASRTL_ERR_LOG(
                            "asrtio", "Failed to initialize server: %s", uv_strerror( r ) );
                        return status::init_failed;
                }

                r = uv_tcp_bind( &server, (const struct sockaddr*) &addr, 0 );
                if ( r != 0 ) {
                        ASRTL_ERR_LOG(
                            "asrtio", "Failed to bind to address: %s", uv_strerror( r ) );
                        return status::bind_failed;
                }
                server.data = this;
                uv_idle_init( server.loop, &idle );
                idle.data = this;
                uv_idle_start( &idle, []( uv_idle_t* h ) {
                        static_cast< rsim_ctx* >( h->data )->tick();
                } );
                r = uv_listen( (uv_stream_t*) &server, 128, []( uv_stream_t* server, int status ) {
                        if ( status < 0 ) {
                                ASRTL_ERR_LOG(
                                    "test_rsim", "Listen error: %s", uv_strerror( status ) );
                                return;
                        }
                        auto& self = *static_cast< rsim_ctx* >( server->data );
                        auto& ctx  = self.conns.emplace_back( self.seed );
                        uv_tcp_init( server->loop, &ctx.client );
                        if ( uv_accept( server, (uv_stream_t*) &ctx.client ) == 0 ) {
                                ASRTL_INF_LOG( "test_rsim", "Accepted connection" );
                                ctx.start();
                        } else {
                                ctx.close();
                        }
                } );
                if ( r != 0 ) {
                        ASRTL_ERR_LOG( "test_rsim", "uv_listen failed: %s", uv_strerror( r ) );
                        return status::listen_failed;
                }
                return status::success;
        }

        void close()
        {
                if ( closed )
                        return;
                closed = true;
                uv_idle_stop( &idle );
                uv_close( (uv_handle_t*) &idle, nullptr );
                for ( auto& c : conns )
                        c.close();
                uv_close( (uv_handle_t*) &server, nullptr );
        }

        friend task< void > async_close( task_ctx&, rsim_ctx& );
};

inline task< void > async_close( task_ctx&, rsim_ctx& rs )
{
        if ( rs.closed )
                co_return;
        rs.closed = true;
        uv_idle_stop( &rs.idle );
        co_await uv_close_handle{ (uv_handle_t*) &rs.idle };
        for ( auto& c : rs.conns ) {
                if ( !c.closed ) {
                        c.closed = true;
                        co_await uv_close_handle{ (uv_handle_t*) &c.client };
                }
        }
        co_await uv_close_handle{ (uv_handle_t*) &rs.server };
}

}  // namespace asrtio
