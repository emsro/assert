#pragma once

#include "../asrtc/status_to_str.h"
#include "../asrtcpp/controller.hpp"
#include "../asrtl/log.h"
#include "../asrtlpp/fmt.hpp"
#include "../asrtlpp/util.hpp"
#include "./util.hpp"

#include <chrono>
#include <uv.h>

namespace asrtio
{

struct cntr_tcp_sys
{

        cntr_tcp_sys( uv_loop_t* l )
          : loop( l )
          , tasks( l )
        {
                connect_req.data = this;
        }

        void init( sockaddr_in& dest )
        {
                std::ignore = uv_tcp_init( loop, &client );

                std::ignore = uv_tcp_connect(
                    &connect_req,
                    &client,
                    (const struct sockaddr*) &dest,
                    []( uv_connect_t* req, int status ) {
                            if ( status < 0 ) {
                                    ASRTL_ERR_LOG(
                                        "asrtio_main",
                                        "Connection error: %s",
                                        uv_strerror( status ) );
                                    return;
                            }
                            auto& sys = *static_cast< cntr_tcp_sys* >( req->data );
                            sys.start();
                            ASRTL_INF_LOG( "asrtio_main", "Connected to the system" );
                    } );
        }

        void schedule_for_each_test(
            std::function< void( uint32_t id, std::string_view name ) > cb,
            std::function< void( uint32_t ) >                           on_count    = {},
            std::function< void() >                                     on_complete = {} )
        {
                auto p         = std::make_unique< test_pool_task >( cntr, std::move( cb ) );
                p->on_count    = std::move( on_count );
                p->on_complete = std::move( on_complete );
                tasks.push( std::move( p ) );
        }

        void schedule_run_test(
            uint32_t                                      id,
            std::function< void() >                       on_start  = {},
            std::function< void( asrtc::result const& ) > on_result = {} )
        {
                auto p = std::make_unique< run_test_task >(
                    cntr, id, std::move( on_start ), std::move( on_result ) );
                tasks.push( std::move( p ) );
        }

        void schedule_call( std::function< void() > func )
        {
                tasks.push( std::make_unique< call_function_task >( std::move( func ) ) );
        }

        void tick()
        {
                if ( auto s = cntr.tick(); s != ASRTC_SUCCESS )
                        ASRTL_ERR_LOG(
                            "asrtio_main", "Controller tick failed: %s", asrtc_status_to_str( s ) );
        }


        void start()
        {
                uv_idle_init( loop, &idle_handle );
                idle_handle.data = this;
                uv_idle_start( &idle_handle, []( uv_idle_t* h ) {
                        static_cast< cntr_tcp_sys* >( h->data )->tick();
                } );
                rx.start( (uv_stream_t*) &client, cntr.node(), [this]( ssize_t nread ) {
                        if ( nread == UV_EOF )
                                ASRTL_DBG_LOG( "asrtio_main", "Connection closed by remote" );
                        else
                                ASRTL_ERR_LOG(
                                    "asrtio_main",
                                    "Read error: %s",
                                    uv_strerror( static_cast< int >( nread ) ) );
                        close();
                } );
        }

        void close()
        {
                if ( closed )
                        return;
                closed = true;
                uv_idle_stop( &idle_handle );
                uv_close( (uv_handle_t*) &idle_handle, nullptr );
                tasks.stop();
                uv_close( (uv_handle_t*) &client, nullptr );
        }

private:
        bool              closed = false;
        uv_loop_t*        loop;
        uv_connect_t      connect_req;
        uv_idle_t         idle_handle;
        uv_tcp_t          client;
        asrtc::controller cntr{
            [&]( asrtl::chann_id id, asrtl::rec_span& buff ) -> asrtl::status {
                    return rx.write( (uv_stream_t*) &client, id, buff );
            },
            [this]( asrtl::source sr, asrtl::ecode ec ) -> asrtc::status {
                    auto s = std::format( "Source: {}, code: {}", sr, ec );
                    ASRTL_ERR_LOG( "asrtio_main", "%s", s.c_str() );
                    close();
                    return ASRTC_SUCCESS;
            },
            [this]( asrtc::status s ) -> asrtc::status {
                    if ( s != ASRTC_SUCCESS )
                            ASRTL_ERR_LOG(
                                "asrtio_main",
                                "Controller init failed: %s",
                                asrtc_status_to_str( s ) );
                    tasks.start();
                    return s;
            } };

        uv_tasks  tasks;
        cobs_node rx;
};

inline std::shared_ptr< cntr_tcp_sys > make_tcp_sys(
    uv_loop_t*       loop,
    std::string_view host,
    uint16_t         port )
{
        struct sockaddr_in dest;
        if ( uv_ip4_addr( host.data(), port, &dest ) != 0 ) {
                ASRTL_ERR_LOG( "asrtio_main", "Invalid address: %s:%u", host.data(), port );
                return nullptr;
        }
        std::shared_ptr< cntr_tcp_sys > sys = std::make_shared< cntr_tcp_sys >( loop );
        sys->init( dest );
        return sys;
}

struct suite_reporter
{
        virtual void on_count( uint32_t total )                                             = 0;
        virtual void on_test_start( std::string_view name )                                 = 0;
        virtual void on_test_done( std::string_view name, bool passed, double duration_ms ) = 0;
        virtual ~suite_reporter() = default;
};

void run_test_suite(
    cntr_tcp_sys&           cntr,
    suite_reporter&         reporter,
    std::function< void() > on_done = {} )
{
        struct state_t
        {
                uint32_t                                             total  = 0;
                int                                                  failed = 0;
                std::vector< std::chrono::steady_clock::time_point > starts;
                std::function< void() >                              on_done_cb;
        };

        auto state        = std::make_shared< state_t >();
        state->on_done_cb = std::move( on_done );

        cntr.schedule_for_each_test(
            [&cntr, &reporter, state]( uint32_t id, std::string_view name ) {
                    std::string n{ name };
                    cntr.schedule_run_test(
                        id,
                        [&reporter, state, id, n] {
                                reporter.on_test_start( n );
                                state->starts[id] = std::chrono::steady_clock::now();
                        },
                        [&reporter, state, n]( asrtc::result const& res ) {
                                using namespace std::chrono;
                                double ms = duration_cast< duration< double, std::milli > >(
                                                steady_clock::now() - state->starts[res.test_id] )
                                                .count();
                                bool passed = ( res.res == ASRTC_TEST_SUCCESS );
                                if ( !passed )
                                        ++state->failed;
                                reporter.on_test_done( n, passed, ms );
                        } );
            },
            [&reporter, state]( uint32_t count ) {
                    state->total = count;
                    state->starts.resize( count );
                    reporter.on_count( count );
            },
            [state, &cntr] {
                    cntr.schedule_call( [state] {
                            ASRTL_INF_LOG( "asrtio_main", "All tasks completed, shutting down" );
                            if ( state->on_done_cb )
                                    state->on_done_cb();
                    } );
            } );
}

}  // namespace asrtio
