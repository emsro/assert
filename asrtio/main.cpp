
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
#include "../asrtl/log.h"
#include "../asrtl/stream_proto.h"
#include "../asrtlpp/fmt.hpp"
#include "./cntr_tcp_sys.hpp"
#include "./euv.hpp"
#include "./output_fs.hpp"
#include "./pbar.hpp"
#include "./real_fs.hpp"
#include "./rsim.hpp"
#include "./util.hpp"

#include <CLI/CLI.hpp>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <list>
#include <memory>
#include <sstream>
#include <uv.h>
#include <vector>

using namespace std::literals::chrono_literals;

namespace asrtio
{
using asrt::opt;
namespace
{
struct pbar_reporter : suite_reporter
{
        pbar::terminal_progress& bar;
        int                      done   = 0;
        int                      failed = 0;

        explicit pbar_reporter( pbar::terminal_progress& b )
          : bar( b )
        {
        }

        void on_count( uint32_t total ) override { bar.set_total( (int) total ); }

        void on_test_start( std::string_view name, uint32_t run_idx, uint32_t run_total ) override
        {
                auto label = std::string{ name };
                if ( run_total > 1 )
                        label +=
                            " " + std::to_string( run_idx ) + "/" + std::to_string( run_total );
                bar.set_status( label );
        }

        void on_test_done(
            std::string_view name,
            bool             passed,
            double           duration_ms,
            uint32_t         run_idx,
            uint32_t         run_total ) override
        {
                if ( !passed )
                        ++failed;
                auto label = std::string{ name };
                if ( run_total > 1 )
                        label +=
                            " " + std::to_string( run_idx ) + "/" + std::to_string( run_total );
                bar.log_result( label, passed, duration_ms );
                bar.set_progress( ++done, failed );
        }

        void on_diagnostic( std::string_view file, uint32_t line, std::string_view extra ) override
        {
                auto loc = std::string{ file } + ":" + std::to_string( line );
                if ( !extra.empty() )
                        loc += " " + std::string{ extra };
                bar.log( pbar::colored_wall_time() + "    " + pbar::fg( loc, pbar::colors::red ) );
        }

        void on_collect_data( std::string_view, asrt_flat_tree const* ) override {}

        void on_stream_data( std::string_view, asrt::stream_schemas const& ) override {}
};

std::shared_ptr< pbar::terminal_progress > g_bar;
asrt_log_level                             g_log_level = ASRT_LOG_ERROR;
std::ostream*                              g_log_file  = nullptr;

std::string plain_log_line(
    enum asrt_log_level level,
    char const*         module,
    char const*         fmt,
    va_list             args )
{
        char msgbuf[1024];
        vsnprintf( msgbuf, sizeof( msgbuf ), fmt, args );
        char const* ls;
        if ( level == ASRT_LOG_ERROR )
                ls = "ERROR";
        else if ( level == ASRT_LOG_INFO )
                ls = "INFO ";
        else
                ls = "DEBUG";
        auto now   = std::chrono::system_clock::now();
        auto now_t = std::chrono::system_clock::to_time_t( now );
        auto us =
            std::chrono::duration_cast< std::chrono::microseconds >( now.time_since_epoch() ) %
            std::chrono::seconds( 1 );
        struct tm ti
        {
        };
        localtime_r( &now_t, &ti );
        char ts[16];
        std::snprintf(
            ts,
            sizeof( ts ),
            "%02d%02d%02d.%06d",
            ti.tm_hour,
            ti.tm_min,
            ti.tm_sec,
            static_cast< int >( us.count() ) );
        return std::string( ts ) + "  " + ( module ? module : "-" ) + "  " + ls + "  " + msgbuf;
}

std::string pbar_format_log(
    enum asrt_log_level level,
    char const*         module,
    char const*         fmt,
    va_list             args )
{
        char msgbuf[1024];
        vsnprintf( msgbuf, sizeof( msgbuf ), fmt, args );
        pbar::color lc;
        char const* ls;
        if ( level == ASRT_LOG_ERROR ) {
                lc = pbar::colors::red;
                ls = "ERROR";
        } else if ( level == ASRT_LOG_INFO ) {
                lc = pbar::colors::green;
                ls = "INFO ";
        } else {
                lc = pbar::colors::dim_gray;
                ls = "DEBUG";
        }
        return pbar::colored_wall_time() + "  " + pbar::dim( module ? module : "-" ) + "  " +
               pbar::fg( ls, lc ) + "  " + msgbuf;
}
}  // namespace

extern "C" {
void asrt_log( enum asrt_log_level level, char const* module, char const* fmt, ... )
{
        if ( g_log_file ) {
                va_list fargs;
                va_start( fargs, fmt );
                auto fline = plain_log_line( level, module, fmt, fargs );
                va_end( fargs );
                *g_log_file << fline << '\n';
        }
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

namespace
{

task< void > run_tcp(
    task_ctx&                       ctx,
    arena&                          arena,
    steady_clock&                   clk,
    uv_loop_t*                      loop,
    char const*                     host,
    uint16_t                        port,
    std::chrono::milliseconds       timeout,
    std::unique_ptr< param_config > params,
    output_fs&                      fs,
    std::filesystem::path           output_dir )
{
        pbar_reporter reporter{ *g_bar };
        auto          client = std::make_shared< uv_tcp_t >();
        if ( auto r = uv_tcp_init( loop, client.get() ); r != 0 ) {
                ASRT_ERR_LOG( "asrtio", "uv_tcp_init failed: %s", uv_strerror( r ) );
                co_await ecor::just_error( ASRT_INIT_ERR );
        }
        co_await tcp_connect{ client.get(), host, port };
        auto sys = arena.make< cntr_tcp_sys >( client, clk );
        sys->start();

        co_await run_test_suite( ctx, *sys, reporter, timeout, *params, fs, output_dir );
        g_bar->finish();
        g_bar.reset();
};

task< void > run_rsim(
    task_ctx&                       ctx,
    arena&                          arena,
    steady_clock&                   clk,
    uv_loop_t*                      loop,
    uint32_t                        seed,
    std::chrono::milliseconds       timeout,
    std::unique_ptr< param_config > params,
    output_fs&                      fs,
    std::filesystem::path           output_dir )
{
        pbar_reporter reporter{ *g_bar };
        auto          rs = arena.make< rsim_ctx >( loop, seed );
        rs->start();

        auto client = std::make_shared< uv_tcp_t >();
        if ( auto r = uv_tcp_init( loop, client.get() ); r != 0 ) {
                ASRT_ERR_LOG( "asrtio", "uv_tcp_init failed: %s", uv_strerror( r ) );
                co_await ecor::just_error( ASRT_INIT_ERR );
        }
        co_await tcp_connect{ client.get(), "0.0.0.0", rs->port() };
        auto sys = arena.make< cntr_tcp_sys >( client, clk );
        sys->start();

        co_await run_test_suite( ctx, *sys, reporter, timeout, *params, fs, output_dir );
        ASRT_INF_LOG( "asrtio", "run test suite finished" );
        g_bar->finish();
        g_bar.reset();
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
                ASRT_INF_LOG( "asrtio_main", "Task completed successfully" );
                stop_idle();
        }

        void set_error( ecor::task_error )
        {
                ASRT_ERR_LOG( "asrtio_main", "Task error" );
                stop_idle();
        }

        void set_error( asrt::status s )
        {
                ASRT_ERR_LOG( "asrtio_main", "Task error: %s", asrt_status_to_str( s ) );
                stop_idle();
        }

        void set_stopped()
        {
                ASRT_INF_LOG( "asrtio_main", "Task stopped" );
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
}  // namespace
}  // namespace asrtio

int main( int argc, char* argv[] )
{
        using namespace asrtio;
        uv_loop_t* loop = uv_default_loop();
        std::optional< asrtio::complete_arena_connect_result< task< void >, final_receiver > > t;
        asrt::malloc_free_memory_resource mem_res;
        real_fs                           rfs;
        null_fs                           nfs;
        task_ctx                          ctx{ mem_res };
        arena                             ar{ ctx, mem_res };
        steady_clock                      clk;
        uv_idle_t                         idle;
        CLI::App                          app{ "App description" };
        argv = app.ensure_utf8( argv );

        auto opt       = std::make_shared< tcp_opts >();
        int  verbosity = 0;
        app.add_flag( "-v,--verbose", verbosity, "Verbosity: -v = info, -vv = debug" );
        app.require_subcommand( 1, 1 );

        auto* sub = app.add_subcommand( "tcp", "Connect to TCP-based system" );
        sub->fallthrough();

        sub->add_option( "-p,--port", opt->port, "Port to connect to" )->required();
        sub->add_option( "--host", opt->host, "Host to connect to" )->required();

        uint32_t    timeout_ms = 5000U;
        std::string params_file;
        std::string output_dir;
        app.add_option( "--timeout", timeout_ms, "Timeout in milliseconds" );
        app.add_option( "--params", params_file, "Path to JSON param config file" );
        app.add_option( "--output", output_dir, "Output directory for test data files" );

        auto launch = [&]( auto make_task ) {
                auto timeout = std::chrono::milliseconds{ timeout_ms };
                if ( output_dir.empty() )
                        ASRT_INF_LOG(
                            "asrtio", "No --output specified; data files will not be written" );
                auto params = std::make_unique< param_config >();
                if ( !params_file.empty() ) {
                        params = param_config_from_file( params_file );
                        if ( !params ) {
                                std::fprintf( stderr, "Failed to load param config\n" );
                                std::exit( 1 );
                        }
                }
                g_bar  = std::make_shared< pbar::terminal_progress >();
                auto s = make_task( timeout, std::move( params ) ) | asrtio::complete_arena( ar );
                t.emplace( std::move( s ).connect( final_receiver{ &idle } ) );
        };

        sub->callback( [&, opt] {
                launch( [&, opt]( auto timeout, auto params ) {
                        return run_tcp(
                            ctx,
                            ar,
                            clk,
                            loop,
                            opt->host.data(),
                            opt->port,
                            timeout,
                            std::move( params ),
                            output_dir.empty() ? static_cast< output_fs& >( nfs ) : rfs,
                            output_dir );
                } );
        } );

        auto* rsim      = app.add_subcommand( "rsim", "Run the test RSIM system" );
        auto  rsim_seed = std::make_shared< uint32_t >( 42U );
        rsim->add_option( "--seed", *rsim_seed, "Seed for pseudo-random test simulation" );
        rsim->fallthrough();
        rsim->callback( [&, rsim_seed] {
                launch( [&, rsim_seed]( auto timeout, auto params ) {
                        if ( params_file.empty() ) {
                                std::istringstream in( R"({
                                        "*": {"default": 1},
                                        "demo_param_value": [{"val": 10}, {"val": 20}],
                                        "demo_param_count": {"a": 1, "b": 2, "c": 3}
                                })" );
                                params = param_config_from_stream( in );
                        }
                        return run_rsim(
                            ctx,
                            ar,
                            clk,
                            loop,
                            *rsim_seed,
                            timeout,
                            std::move( params ),
                            output_dir.empty() ? static_cast< output_fs& >( nfs ) : rfs,
                            output_dir );
                } );
        } );

        CLI11_PARSE( app, argc, argv );

        if ( verbosity >= 2 )
                g_log_level = ASRT_LOG_DEBUG;
        else if ( verbosity == 1 )
                g_log_level = ASRT_LOG_INFO;
        else
                g_log_level = ASRT_LOG_ERROR;

        std::optional< file_writer > log_writer;
        if ( !output_dir.empty() ) {
                rfs.create_directories( output_dir );
                log_writer.emplace(
                    rfs.open_write( std::filesystem::path{ output_dir } / "asrtio.log" ) );
                g_log_file = &log_writer->stream();
        }

        idle.data = &ctx;
        uv_idle_init( loop, &idle );
        uv_idle_start( &idle, []( uv_idle_t* handle ) {
                auto& ctx = *static_cast< task_ctx* >( handle->data );
                ctx.tick();
        } );

        if ( t )
                t->start();

        uv_run( loop, UV_RUN_DEFAULT );
        uv_loop_close( loop );

        g_log_file = nullptr;
        log_writer.reset();

        return 0;
}
