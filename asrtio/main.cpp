
#include "../asrtl/log.h"
#include "../asrtlpp/fmt.hpp"
#include "./cntr_tcp_sys.hpp"
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

std::shared_ptr< rsym_sys > make_rsym_sys( uv_loop_t* loop, uint32_t seed )
{
        auto rsim = make_rsim( loop, seed );
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
                g_bar         = std::make_shared< pbar::terminal_progress >();
                auto reporter = std::make_shared< pbar_reporter >( *g_bar );
                run_test_suite( *sys, *reporter, [reporter, sys] {
                        g_bar->finish();
                        g_bar.reset();
                        sys->close();
                } );
        } );

        auto* rsim      = app.add_subcommand( "rsim", "Run the test RSIM system" );
        auto  rsim_seed = std::make_shared< uint32_t >( 42u );
        rsim->add_option( "--seed", *rsim_seed, "Seed for pseudo-random test simulation" );
        rsim->fallthrough();
        rsim->callback( [loop, &sys, rsim_seed] {
                auto rsys = make_rsym_sys( loop, *rsim_seed );
                if ( !rsys ) {
                        ASRTL_ERR_LOG( "asrtio_main", "Failed to create RSIM system" );
                        return;
                }
                sys           = std::shared_ptr< cntr_tcp_sys >( rsys, rsys->sys.get() );
                g_bar         = std::make_shared< pbar::terminal_progress >();
                auto reporter = std::make_shared< pbar_reporter >( *g_bar );
                run_test_suite( *sys, *reporter, [reporter, sys, sim = rsys->sim] {
                        g_bar->finish();
                        g_bar.reset();
                        sys->close();
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
