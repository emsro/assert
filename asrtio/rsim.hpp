#pragma once

#include "../asrtl/chann.h"
#include "../asrtl/cobs.h"
#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"
#include "../asrtr/status_to_str.h"
#include "../asrtrpp/reactor.hpp"
#include "./util.hpp"

#include <chrono>
#include <list>
#include <optional>
#include <random>
#include <uv.h>

namespace asrtio
{

struct utest_sim
{
        int              duration_ms = 300;
        int              variance_ms = 0;
        asrtr_test_state res         = ASRTR_TEST_PASS;
        bool             randomize   = false;
        std::string      tname;

        std::mt19937* rng_ptr = nullptr;

        using clock = std::chrono::steady_clock;
        std::optional< clock::time_point > start;
        int                                actual_ms = 0;

        char const* name()
        {
                return tname.data();
        }

        asrtr::status operator()( asrtr::record& r )
        {
                if ( !start ) {
                        start = clock::now();
                        if ( variance_ms > 0 ) {
                                std::uniform_int_distribution< int > jitter(
                                    -variance_ms, variance_ms );
                                actual_ms = std::max( 0, duration_ms + jitter( *rng_ptr ) );
                        } else {
                                actual_ms = duration_ms;
                        }
                        if ( randomize ) {
                                static constexpr asrtr_test_state k_results[] = {
                                    ASRTR_TEST_PASS, ASRTR_TEST_FAIL, ASRTR_TEST_ERROR };
                                std::uniform_int_distribution< int > pick( 0, 2 );
                                res = k_results[pick( *rng_ptr )];
                        }
                }

                auto elapsed =
                    std::chrono::duration_cast< std::chrono::milliseconds >( clock::now() - *start )
                        .count();
                if ( elapsed >= actual_ms )
                        r.state = res;
                else
                        r.state = ASRTR_TEST_RUNNING;
                return ASRTR_SUCCESS;
        }
};


struct conn_ctx
{
        uv_tcp_t                              client;
        bool                                  closed = false;
        std::mt19937                          rng;
        asrtr::reactor                        reac;
        std::list< asrtr::unit< utest_sim > > tests;

        conn_ctx( uint32_t seed )
          : rng( seed )
          , reac( *this, "simulator reactor" )
        {
                client.data = this;

                reg( utest_sim{
                    .duration_ms = 300,
                    .variance_ms = 200,
                    .res         = ASRTR_TEST_PASS,
                    .randomize   = true,
                    .tname       = "utest_sim" } );
                reg( utest_sim{
                    .duration_ms = 500,
                    .variance_ms = 300,
                    .res         = ASRTR_TEST_FAIL,
                    .randomize   = true,
                    .tname       = "utest_sim_fail" } );
                reg( utest_sim{
                    .duration_ms = 150,
                    .variance_ms = 100,
                    .res         = ASRTR_TEST_ERROR,
                    .randomize   = false,
                    .tname       = "utest_sim_error" } );
                reg( utest_sim{
                    .duration_ms = 0, .res = ASRTR_TEST_PASS, .tname = "utest_sim_insta_pass" } );
                reg( utest_sim{
                    .duration_ms = 0, .res = ASRTR_TEST_FAIL, .tname = "utest_sim_insta_fail" } );
                reg( utest_sim{
                    .duration_ms = 0, .res = ASRTR_TEST_ERROR, .tname = "utest_sim_insta_error" } );
        }

        void reg( utest_sim sim )
        {
                sim.rng_ptr = &rng;
                auto& t     = tests.emplace_back( std::move( sim ) );
                reac.add_test( t );
        }

        asrtl::status operator()( asrtl::chann_id id, asrtl::rec_span* buff )
        {
                return rx.write( (uv_stream_t*) &client, id, *buff );
        }

        void tick()
        {
                auto s = reac.tick();
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
                if ( !closed ) {
                        ASRTL_INF_LOG( "asrtio", "Closing rsim reactor connection" );
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
        uint32_t              seed   = 0;
        bool                  closed = false;
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
                            auto& ctx  = self.conns.emplace_back( self.seed );
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
                if ( closed )
                        return;
                closed = true;
                uv_idle_stop( &idle );
                uv_close( (uv_handle_t*) &idle, nullptr );
                for ( auto& c : conns )
                        c.close();
                uv_close( (uv_handle_t*) &server, nullptr );
        }
};

inline std::shared_ptr< asrtio::rsim_ctx > make_rsim( uv_loop_t* loop, uint32_t seed = 42 )
{
        std::shared_ptr< rsim_ctx > ctx = std::make_shared< rsim_ctx >();
        ctx->seed                       = seed;
        struct sockaddr_in addr;
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
