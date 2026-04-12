
#include "../asrtl/log.h"
#include "../asrtl/stream_proto.h"
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
#include <cstring>
#include <list>
#include <memory>
#include <sstream>
#include <uv.h>
#include <vector>

using namespace std::literals::chrono_literals;

namespace asrtio
{
using asrtl::opt;

static std::string flat_node_json( asrtl_flat_tree* tree, asrtl_flat_id id )
{
        asrtl_flat_query_result res;
        if ( asrtl_flat_tree_query( tree, id, &res ) != ASRTL_SUCCESS )
                return "null";

        switch ( res.value.type ) {
        case ASRTL_FLAT_STYPE_STR: {
                std::string s = "\"";
                if ( res.value.data.s.str_val )
                        for ( char const* p = res.value.data.s.str_val; *p; ++p ) {
                                if ( *p == '"' )
                                        s += "\\\"";
                                else if ( *p == '\\' )
                                        s += "\\\\";
                                else
                                        s += *p;
                        }
                return s + "\"";
        }
        case ASRTL_FLAT_STYPE_U32:
                return std::to_string( res.value.data.s.u32_val );
        case ASRTL_FLAT_STYPE_I32:
                return std::to_string( res.value.data.s.i32_val );
        case ASRTL_FLAT_STYPE_FLOAT:
                return std::to_string( res.value.data.s.float_val );
        case ASRTL_FLAT_STYPE_BOOL:
                return res.value.data.s.bool_val ? "true" : "false";
        case ASRTL_FLAT_STYPE_NULL:
        case ASRTL_FLAT_STYPE_NONE:
                return "null";
        case ASRTL_FLAT_CTYPE_OBJECT: {
                std::string s  = "{";
                auto        cid = res.value.data.cont.first_child;
                bool        first = true;
                while ( cid != 0 ) {
                        asrtl_flat_query_result child;
                        if ( asrtl_flat_tree_query( tree, cid, &child ) != ASRTL_SUCCESS )
                                break;
                        if ( !first )
                                s += ",";
                        first = false;
                        if ( child.key )
                                s += std::string( "\"" ) + child.key + "\":";
                        s += flat_node_json( tree, cid );
                        cid = child.next_sibling;
                }
                return s + "}";
        }
        case ASRTL_FLAT_CTYPE_ARRAY: {
                std::string s  = "[";
                auto        cid = res.value.data.cont.first_child;
                bool        first = true;
                while ( cid != 0 ) {
                        asrtl_flat_query_result child;
                        if ( asrtl_flat_tree_query( tree, cid, &child ) != ASRTL_SUCCESS )
                                break;
                        if ( !first )
                                s += ",";
                        first = false;
                        s += flat_node_json( tree, cid );
                        cid = child.next_sibling;
                }
                return s + "]";
        }
        default:
                return "null";
        }
}

static char const* strm_field_label( asrtl_strm_field_type t )
{
        switch ( (enum asrtl_strm_field_type_e) t ) {
        case ASRTL_STRM_FIELD_U8:
                return "u8";
        case ASRTL_STRM_FIELD_U16:
                return "u16";
        case ASRTL_STRM_FIELD_U32:
                return "u32";
        case ASRTL_STRM_FIELD_I8:
                return "i8";
        case ASRTL_STRM_FIELD_I16:
                return "i16";
        case ASRTL_STRM_FIELD_I32:
                return "i32";
        case ASRTL_STRM_FIELD_FLOAT:
                return "float";
        case ASRTL_STRM_FIELD_BOOL:
                return "bool";
        default:
                return "?";
        }
}

static std::string decode_strm_field( uint8_t*& p, asrtl_strm_field_type t )
{
        switch ( (enum asrtl_strm_field_type_e) t ) {
        case ASRTL_STRM_FIELD_U8:
                return std::to_string( *p++ );
        case ASRTL_STRM_FIELD_I8:
                return std::to_string( static_cast< int8_t >( *p++ ) );
        case ASRTL_STRM_FIELD_BOOL:
                return ( *p++ ) ? "true" : "false";
        case ASRTL_STRM_FIELD_U16: {
                uint16_t v;
                asrtl_cut_u16( &p, &v );
                return std::to_string( v );
        }
        case ASRTL_STRM_FIELD_I16: {
                uint16_t v;
                asrtl_cut_u16( &p, &v );
                return std::to_string( static_cast< int16_t >( v ) );
        }
        case ASRTL_STRM_FIELD_U32: {
                uint32_t v;
                asrtl_cut_u32( &p, &v );
                return std::to_string( v );
        }
        case ASRTL_STRM_FIELD_I32: {
                int32_t v;
                asrtl_cut_i32( &p, &v );
                return std::to_string( v );
        }
        case ASRTL_STRM_FIELD_FLOAT: {
                uint32_t bits;
                asrtl_cut_u32( &p, &bits );
                float v;
                std::memcpy( &v, &bits, sizeof( v ) );
                return std::to_string( v );
        }
        default:
                return "?";
        }
}

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

        void on_collect_data( std::string_view name, asrtl_flat_tree const* tree ) override
        {
                auto json = flat_node_json( const_cast< asrtl_flat_tree* >( tree ), 1 );
                bar.log( pbar::colored_wall_time() + "    " +
                         pbar::fg( std::string{ name }, pbar::colors::cyan ) +
                         " collect: " + json );
        }

        void on_stream_data(
            std::string_view              name,
            asrtc::stream_schemas const& schemas ) override
        {
                for ( uint32_t si = 0; si < schemas->schema_count; ++si ) {
                        auto const& s = schemas->schemas[si];
                        std::string csv;
                        for ( uint8_t fi = 0; fi < s.field_count; ++fi ) {
                                if ( fi > 0 )
                                        csv += ',';
                                csv += strm_field_label( s.fields[fi] );
                        }
                        csv += '\n';
                        for ( auto const* rec = s.first; rec; rec = rec->next ) {
                                auto* p = rec->data;
                                for ( uint8_t fi = 0; fi < s.field_count; ++fi ) {
                                        if ( fi > 0 )
                                                csv += ',';
                                        csv += decode_strm_field( p, s.fields[fi] );
                                }
                                csv += '\n';
                        }
                        bar.log( pbar::colored_wall_time() + "    " +
                                 pbar::fg( std::string{ name }, pbar::colors::cyan ) +
                                 " stream[" + std::to_string( s.schema_id ) + "]:\n" + csv );
                }
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
    task_ctx&                       ctx,
    arena&                          arena,
    steady_clock&                   clk,
    uv_loop_t*                      loop,
    std::string_view                host,
    uint16_t                        port,
    std::chrono::milliseconds       timeout,
    std::unique_ptr< param_config > params )
{
        pbar_reporter reporter{ *g_bar };
        auto          client = std::make_shared< uv_tcp_t >();
        if ( auto r = uv_tcp_init( loop, client.get() ); r != 0 ) {
                ASRTL_ERR_LOG( "asrtio", "uv_tcp_init failed: %s", uv_strerror( r ) );
                co_await ecor::just_error( status::init_failed );
        }
        co_await tcp_connect{ client.get(), host, port };
        auto sys = arena.make< cntr_tcp_sys >( client, clk );
        sys->start();

        co_await run_test_suite( ctx, *sys, reporter, timeout, *params );
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
    std::unique_ptr< param_config > params )
{
        pbar_reporter reporter{ *g_bar };
        auto          rs = arena.make< rsim_ctx >( loop, seed );
        rs->start();

        auto client = std::make_shared< uv_tcp_t >();
        if ( auto r = uv_tcp_init( loop, client.get() ); r != 0 ) {
                ASRTL_ERR_LOG( "asrtio", "uv_tcp_init failed: %s", uv_strerror( r ) );
                co_await ecor::just_error( status::init_failed );
        }
        co_await tcp_connect{ client.get(), "0.0.0.0", rs->port() };
        auto sys = arena.make< cntr_tcp_sys >( client, clk );
        sys->start();

        co_await run_test_suite( ctx, *sys, reporter, timeout, *params );
        ASRTL_INF_LOG( "asrtio", "run test suite finished" );
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
                ASRTL_INF_LOG( "asrtio_main", "Task completed successfully" );
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
                ASRTL_INF_LOG( "asrtio_main", "Task stopped" );
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
        uv_loop_t* loop = uv_default_loop();
        std::optional< asrtio::complete_arena_connect_result< task< void >, final_receiver > > t;
        std::shared_ptr< pbar_reporter >   reporter;
        asrtl::malloc_free_memory_resource mem_res;
        task_ctx                           ctx{ mem_res };
        arena                              ar{ ctx, mem_res };
        steady_clock                       clk;
        uv_idle_t                          idle;
        CLI::App                           app{ "App description" };
        argv = app.ensure_utf8( argv );

        auto opt       = std::make_shared< tcp_opts >();
        int  verbosity = 0;
        app.add_flag( "-v,--verbose", verbosity, "Verbosity: -v = info, -vv = debug" );
        app.require_subcommand( 1, 1 );

        auto* sub = app.add_subcommand( "tcp", "Connect to TCP-based system" );
        sub->fallthrough();

        sub->add_option( "-p,--port", opt->port, "Port to connect to" )->required();
        sub->add_option( "--host", opt->host, "Host to connect to" )->required();

        uint32_t    timeout_ms = 5000u;
        std::string params_file;
        app.add_option( "--timeout", timeout_ms, "Timeout in milliseconds" );
        app.add_option( "--params", params_file, "Path to JSON param config file" );

        auto launch = [&]( auto make_task ) {
                auto timeout = std::chrono::milliseconds{ timeout_ms };
                auto params  = std::make_unique< param_config >();
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
                            opt->host,
                            opt->port,
                            timeout,
                            std::move( params ) );
                } );
        } );

        auto* rsim      = app.add_subcommand( "rsim", "Run the test RSIM system" );
        auto  rsim_seed = std::make_shared< uint32_t >( 42u );
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
                            ctx, ar, clk, loop, *rsim_seed, timeout, std::move( params ) );
                } );
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

        t->start();

        uv_run( loop, UV_RUN_DEFAULT );
        uv_loop_close( loop );

        return 0;
}
