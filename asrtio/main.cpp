
#include "../asrtl/log.h"
#include "../asrtlpp/fmt.hpp"
#include "./cntr_tcp_sys.hpp"
#include "./euv.hpp"
#include "./pbar.hpp"
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

struct pbar_reporter : suite_reporter
{
        pbar::terminal_progress& bar;
        int                      done   = 0;
        int                      failed = 0;

        explicit pbar_reporter( pbar::terminal_progress& b )
          : bar( b )
        {
        }

        void on_count( uint32_t total ) override
        {
                bar.set_total( (int) total );
        }

        void on_test_start( std::string_view name ) override
        {
                bar.set_status( name );
        }

        void on_test_done( std::string_view name, bool passed, double duration_ms ) override
        {
                if ( !passed )
                        ++failed;
                bar.log_result( name, passed, duration_ms );
                bar.set_progress( ++done, failed );
        }

        void on_diagnostic( std::string_view file, uint32_t line ) override
        {
                auto loc = std::string{ file } + ":" + std::to_string( line );
                bar.log( pbar::colored_wall_time() + "    " + pbar::fg( loc, pbar::colors::red ) );
        }
};

std::shared_ptr< pbar::terminal_progress > g_bar;
asrtl_log_level                            g_log_level = ASRTL_LOG_ERROR;

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

task< void > run_tcp(
    task_ctx&                 ctx,
    uv_loop_t*                loop,
    std::string_view          host,
    uint16_t                  port,
    std::chrono::milliseconds timeout )
{
        pbar_reporter reporter{ *g_bar };
        uv_tcp_t      client;
        std::ignore = uv_tcp_init( loop, &client );
        co_await tcp_connect{ &client, host, port };
        steady_clock clk;
        cntr_tcp_sys sys{ &client, clk };
        sys.start( timeout );

        co_await run_test_suite( ctx, sys, reporter, timeout );
        g_bar->finish();
        g_bar.reset();

        sys.close();
};

task< void > run_rsim(
    task_ctx&                 ctx,
    uv_loop_t*                loop,
    uint32_t                  seed,
    std::chrono::milliseconds timeout )
{
        pbar_reporter reporter{ *g_bar };
        rsim_ctx      rs{ loop, seed };
        rs.start();

        uv_tcp_t client;
        std::ignore = uv_tcp_init( loop, &client );
        co_await tcp_connect{ &client, "0.0.0.0", rs.port() };
        steady_clock clk;
        cntr_tcp_sys sys{ &client, clk };
        sys.start( timeout );

        co_await run_test_suite( ctx, sys, reporter, timeout );
        g_bar->finish();
        g_bar.reset();
        sys.close();
        rs.close();
}

struct tcp_opts
{
        std::string host;
        uint16_t    port;
};

struct final_receiver
{
        using receiver_concept = ecor::receiver_t;
        uv_idle_t* idle        = nullptr;

        void set_value()
        {
                stop_idle();
        }

        void set_error( ecor::task_error )
        {
                ASRTL_ERR_LOG( "asrtio_main", "Task error" );
                stop_idle();
        }

        void set_error( asrtio::status s )
        {
                ASRTL_ERR_LOG( "asrtio_main", "Task error: %s", status_to_str( s ) );
                stop_idle();
        }

        void set_stopped()
        {
                stop_idle();
        }

        void stop_idle()
        {
                if ( idle ) {
                        uv_idle_stop( idle );
                        uv_close( (uv_handle_t*) idle, nullptr );
                        idle = nullptr;
                }
        }
};

}  // namespace asrtio

int main( int argc, char* argv[] )
{
        using namespace asrtio;
        uv_loop_t*                                         loop = uv_default_loop();
        ecor::connect_type< task< void >, final_receiver > t;
        std::shared_ptr< pbar_reporter >                   reporter;
        task_ctx                                           ctx;
        uv_idle_t                                          idle;
        CLI::App                                           app{ "App description" };
        argv = app.ensure_utf8( argv );

        auto opt       = std::make_shared< tcp_opts >();
        int  verbosity = 0;
        app.add_flag( "-v,--verbose", verbosity, "Verbosity: -v = info, -vv = debug" );
        app.require_subcommand( 1, 1 );

        auto* sub = app.add_subcommand( "tcp", "Connect to TCP-based system" );
        sub->fallthrough();

        sub->add_option( "-p,--port", opt->port, "Port to connect to" )->required();
        sub->add_option( "--host", opt->host, "Host to connect to" )->required();

        auto timeout = 200ms;

        sub->callback( [loop, opt, &ctx, &t, &reporter, &idle, timeout] {
                g_bar = std::make_shared< pbar::terminal_progress >();
                t     = run_tcp( ctx, loop, opt->host, opt->port, timeout )
                        .connect( final_receiver{ &idle } );
        } );

        auto* rsim      = app.add_subcommand( "rsim", "Run the test RSIM system" );
        auto  rsim_seed = std::make_shared< uint32_t >( 42u );
        rsim->add_option( "--seed", *rsim_seed, "Seed for pseudo-random test simulation" );
        rsim->fallthrough();
        rsim->callback( [loop, rsim_seed, &ctx, &t, &reporter, &idle, timeout] {
                g_bar = std::make_shared< pbar::terminal_progress >();
                t = run_rsim( ctx, loop, *rsim_seed, timeout ).connect( final_receiver{ &idle } );
        } );

        CLI11_PARSE( app, argc, argv );

        if ( verbosity >= 2 )
                g_log_level = ASRTL_LOG_DEBUG;
        else if ( verbosity == 1 )
                g_log_level = ASRTL_LOG_INFO;
        else
                g_log_level = ASRTL_LOG_ERROR;

        idle.data = &ctx;
        uv_idle_init( loop, &idle );
        uv_idle_start( &idle, []( uv_idle_t* handle ) {
                auto& ctx = *static_cast< task_ctx* >( handle->data );
                ctx.tick();
        } );

        t.start();

        uv_run( loop, UV_RUN_DEFAULT );
        uv_loop_close( loop );

        return 0;
}
