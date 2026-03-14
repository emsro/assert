
#include "../asrtc/status_to_str.h"
#include "../asrtcpp/controller.hpp"
#include "../asrtl/cobs.h"
#include "../asrtl/log.h"
#include "../asrtl/util.h"
#include "../asrtlpp/fmt.hpp"
#include "./deps/pbar.hpp"
#include "./rsim.hpp"
#include "./util.hpp"

#include <CLI/CLI.hpp>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <list>
#include <uv.h>
#include <vector>

using namespace std::literals::chrono_literals;

namespace asrtio
{
using asrtl::opt;

pbar::terminal_progress* g_bar       = nullptr;
asrtl_log_level          g_log_level = ASRTL_LOG_ERROR;

static std::string pbar_format_log(
    enum asrtl_log_level level,
    char const*          module,
    char const*          fmt,
    va_list              args )
{
        char msgbuf[1024];
        vsnprintf( msgbuf, sizeof( msgbuf ), fmt, args );
        pbar::color lc;
        char const* ls;
        if ( level == ASRTL_LOG_ERROR ) {
                lc = pbar::colors::red;
                ls = "ERROR";
        } else if ( level == ASRTL_LOG_INFO ) {
                lc = pbar::colors::green;
                ls = "INFO ";
        } else {
                lc = pbar::colors::dim_gray;
                ls = "DEBUG";
        }
        return pbar::colored_wall_time() + "  " + pbar::dim( module ? module : "-" ) + "  " +
               pbar::fg( ls, lc ) + "  " + msgbuf;
}

extern "C" {
ASRTL_DEFINE_GPOS_LOG_IMPL
void asrtl_log( enum asrtl_log_level level, char const* module, char const* fmt, ... )
{
        if ( level < g_log_level )
                return;
        va_list args;
        va_start( args, fmt );
        auto line = pbar_format_log( level, module, fmt, args );
        va_end( args );
        if ( g_bar )
                g_bar->log( line );
        else
                std::printf( "%s\n", line.c_str() );
}
}

struct cntr_tcp_sys
{
        uv_loop_t*        loop;
        uv_connect_t      connect_req;
        uv_idle_t         idle_handle;
        uv_tcp_t          client;
        asrtc::controller cntr{
            [&]( asrtl::chann_id id, std::span< uint8_t > buff ) -> asrtl::status {
                    return rx.write( (uv_stream_t*) &client, id, buff );
            },
            [&]( asrtl::source sr, asrtl::ecode ec ) -> asrtc::status {
                    auto s = std::format( "Source: {}, code: {}", sr, ec );
                    ASRTL_ERR_LOG( "asrtio_main", "%s", s.c_str() );
                    std::abort();  // XXX: improve
            },
            []( asrtc::status s ) -> asrtc::status {
                    if ( s != ASRTC_SUCCESS )
                            ASRTL_ERR_LOG(
                                "asrtio_main",
                                "Controller init failed: %s",
                                asrtc_status_to_str( s ) );
                    return s;
            } };

        uv_tasks tasks;

        cntr_tcp_sys( uv_loop_t* l )
          : loop( l )
          , tasks( l )
        {
                connect_req.data = this;
        }

        bool is_idle() const
        {
                return cntr.is_idle();
        }

        void tick()
        {
                if ( auto s = cntr.tick(); s != ASRTC_SUCCESS )
                        ASRTL_ERR_LOG(
                            "asrtio_main", "Controller tick failed: %s", asrtc_status_to_str( s ) );
        }

        cobs_node rx;

        void start()
        {
                uv_idle_init( loop, &idle_handle );
                idle_handle.data = this;
                uv_idle_start( &idle_handle, []( uv_idle_t* h ) {
                        static_cast< cntr_tcp_sys* >( h->data )->tick();
                } );
                tasks.start();
                rx.start( (uv_stream_t*) &client, cntr.node(), [&]( ssize_t nread ) {
                        std::ignore = nread;
                        std::abort();
                } );
        }

        void close()
        {
                uv_idle_stop( &idle_handle );
                uv_close( (uv_handle_t*) &idle_handle, nullptr );
                uv_close( (uv_handle_t*) &tasks.idle_handle, nullptr );
                uv_close( (uv_handle_t*) &client, nullptr );
        }
};

void run_test_suite( cntr_tcp_sys& cntr, std::function< void() > on_done = {} )
{
        struct state_t
        {
                std::shared_ptr< pbar::terminal_progress >           bar;
                int                                                  done   = 0;
                int                                                  failed = 0;
                std::vector< std::chrono::steady_clock::time_point > starts;
        };

        auto state = std::make_shared< state_t >();
        state->bar = std::make_shared< pbar::terminal_progress >(
            pbar::bar_config{ .suite_label = "assert" } );
        g_bar = state->bar.get();

        cntr.tasks.push( std::make_unique< after_idle >( cntr.cntr, [&cntr, state] {
                schedule_for_each_test(
                    cntr.tasks,
                    cntr.cntr,
                    [&cntr, state]( uint32_t id, std::string_view name ) {
                            std::string n{ name };
                            schedule_run_test(
                                cntr.tasks,
                                cntr.cntr,
                                id,
                                [state, id, n] {
                                        state->bar->set_status( n );
                                        state->starts[id] = std::chrono::steady_clock::now();
                                },
                                [state, n]( asrtc::result const& res ) {
                                        using namespace std::chrono;
                                        double ms =
                                            duration_cast< duration< double, std::milli > >(
                                                steady_clock::now() - state->starts[res.test_id] )
                                                .count();
                                        bool passed = ( res.res == ASRTC_TEST_SUCCESS );
                                        if ( !passed )
                                                ++state->failed;
                                        state->bar->log_result( n, passed, ms );
                                        state->bar->set_progress( ++state->done, state->failed );
                                } );
                    },
                    [state]( uint32_t count ) {
                            state->bar->set_total( count );
                            state->starts.resize( count );
                    } );
        } ) );

        cntr.tasks.set_on_complete( [&cntr, state, on_done = std::move( on_done )] {
                ASRTL_INF_LOG( "asrtio_main", "All tasks completed, shutting down" );
                state->bar->finish();
                g_bar = nullptr;
                cntr.close();
                if ( on_done )
                        on_done();
        } );
}

std::shared_ptr< cntr_tcp_sys > make_tcp_sys(
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
        uv_tcp_init( loop, &sys->client );

        uv_tcp_connect(
            &sys->connect_req,
            &sys->client,
            (const struct sockaddr*) &dest,
            []( uv_connect_t* req, int status ) {
                    if ( status < 0 ) {
                            ASRTL_ERR_LOG(
                                "asrtio_main", "Connection error: %s", uv_strerror( status ) );
                            return;
                    }
                    auto& sys = *static_cast< cntr_tcp_sys* >( req->data );
                    sys.start();
                    ASRTL_INF_LOG( "asrtio_main", "Connected to the system" );
            } );

        return sys;
}

struct rsym_sys
{
        std::shared_ptr< cntr_tcp_sys >     sys;
        std::shared_ptr< asrtio::rsim_ctx > sim;

        void close()
        {
                if ( sys )
                        sys->close();
                if ( sim )
                        sim->close();
        }
};

std::shared_ptr< rsym_sys > make_rsym_sys( uv_loop_t* loop )
{
        auto rsim = make_rsim( loop );
        if ( !rsim ) {
                ASRTL_ERR_LOG( "asrtio", "Failed to create RSIM system" );
                return nullptr;
        }
        rsim->start();
        auto tcp_sys = make_tcp_sys( loop, "127.0.0.1", rsim->port() );

        std::shared_ptr< rsym_sys > rsys =
            std::make_shared< rsym_sys >( std::move( tcp_sys ), std::move( rsim ) );
        return rsys;
}


struct tcp_opts
{
        std::string host;
        uint16_t    port;
};

}  // namespace asrtio

int main( int argc, char* argv[] )
{
        using namespace asrtio;
        uv_loop_t* loop = uv_default_loop();
        CLI::App   app{ "App description" };
        argv = app.ensure_utf8( argv );
        std::shared_ptr< asrtio::cntr_tcp_sys > sys;

        auto opt       = std::make_shared< tcp_opts >();
        int  verbosity = 0;
        app.add_flag( "-v,--verbose", verbosity, "Verbosity: -v = info, -vv = debug" );
        app.require_subcommand( 1, 1 );

        auto* sub = app.add_subcommand( "tcp", "Connect to TCP-based system" );
        sub->fallthrough();

        sub->add_option( "-p,--port", opt->port, "Port to connect to" )->required();
        sub->add_option( "--host", opt->host, "Host to connect to" )->required();

        sub->callback( [loop, opt, &sys] {
                sys = make_tcp_sys( loop, opt->host, opt->port );
                if ( !sys ) {
                        ASRTL_ERR_LOG( "asrtio_main", "Failed to create system" );
                        return;
                }
                run_test_suite( *sys );
        } );

        auto* rsim = app.add_subcommand( "rsim", "Run the test RSIM system" );
        rsim->fallthrough();
        rsim->callback( [loop, &sys] {
                auto rsys = make_rsym_sys( loop );
                if ( !rsys ) {
                        ASRTL_ERR_LOG( "asrtio_main", "Failed to create RSIM system" );
                        return;
                }
                sys = std::shared_ptr< cntr_tcp_sys >( rsys, rsys->sys.get() );
                run_test_suite( *sys, [sim = rsys->sim] {
                        sim->close();
                } );
        } );

        CLI11_PARSE( app, argc, argv );

        if ( verbosity >= 2 )
                g_log_level = ASRTL_LOG_DEBUG;
        else if ( verbosity == 1 )
                g_log_level = ASRTL_LOG_INFO;
        else
                g_log_level = ASRTL_LOG_ERROR;

        uv_run( loop, UV_RUN_DEFAULT );

        return 0;
}
