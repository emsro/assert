#pragma once

#include "../asrtc/assembly.h"
#include "../asrtc/controller.h"
#include "../asrtcpp/collect.hpp"
#include "../asrtcpp/controller.hpp"
#include "../asrtcpp/diag.hpp"
#include "../asrtcpp/param.hpp"
#include "../asrtcpp/stream.hpp"
#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"
#include "../asrtlpp/fmt.hpp"
#include "../asrtlpp/util.hpp"
#include "./euv.hpp"
#include "./output_fs.hpp"
#include "./param_config.hpp"
#include "./task.hpp"
#include "./util.hpp"

#include <chrono>
#include <cstring>
#include <optional>
#include <set>
#include <uv.h>

namespace asrtio
{

struct cntr_tcp_sys
{

        cntr_tcp_sys( std::shared_ptr< uv_tcp_t > client, clock const& clk )
          : client( client )
          , clk_( clk )
        {
                std::ignore = asrtc_assembly_init(
                    &asm_,
                    asrtl_sender{ .ptr = this, .cb = send_cb },
                    asrtl_default_allocator(),
                    asrtc_error_cb{ .ptr = this, .cb = ecb } );
        }

        static asrtl_status ecb( void* ptr, asrtl::source src, asrtl::ecode ec )
        {
                auto* sys = static_cast< cntr_tcp_sys* >( ptr );
                auto  s   = std::format( "Source: {}, code: {}", src, ec );
                ASRTL_ERR_LOG( "asrtio_main", "%s", s.c_str() );
                sys->disconnect();
                return ASRTL_SUCCESS;
        }

        static asrtl_status send_cb(
            void*              ptr,
            asrtl_chann_id     id,
            asrtl_rec_span*    buff,
            asrtl_send_done_cb done_cb,
            void*              done_ptr )
        {
                auto* sys = static_cast< cntr_tcp_sys* >( ptr );
                if ( sys->disconnected_ ) {
                        if ( done_cb )
                                done_cb( done_ptr, ASRTL_SEND_ERR );
                        return ASRTL_SEND_ERR;
                }
                auto st = sys->rx.write( (uv_stream_t*) sys->client.get(), id, *buff );
                if ( done_cb )
                        done_cb( done_ptr, st );
                return st;
        }

        auto take_diag_record()
        {
                return asrtc_diag_take_record( &asm_.diag );
        }

        clock const& clk() const
        {
                return clk_;
        }

        void tick()
        {
                auto now = static_cast< uint32_t >( clk_.now().count() );
                asrtc_assembly_tick( &asm_, now );
        }


        void start()
        {
                uv_idle_init( client->loop, &idle_handle );
                idle_handle.data = this;
                uv_idle_start( &idle_handle, []( uv_idle_t* h ) {
                        static_cast< cntr_tcp_sys* >( h->data )->tick();
                } );
                rx.start(
                    (uv_stream_t*) client.get(),
                    &asm_.cntr.node,
                    "asrtio_cntr",
                    [this]( ssize_t nread ) {
                            if ( nread == UV_EOF )
                                    ASRTL_DBG_LOG( "asrtio_main", "Connection closed by remote" );
                            else
                                    ASRTL_ERR_LOG(
                                        "asrtio_main",
                                        "Read error: %s",
                                        uv_strerror( static_cast< int >( nread ) ) );
                            ASRTL_INF_LOG(
                                "asrtio_main",
                                "Stopping cntr_tcp_sys system and closing connection" );
                            disconnect();
                    } );
        }

        void disconnect()
        {
                if ( disconnected_ )
                        return;
                disconnected_ = true;
                uv_close( (uv_handle_t*) client.get(), nullptr );
        }

        asrtc_controller& cntr()
        {
                return asm_.cntr;
        }

        asrtc::stream_schemas stream_take()
        {
                return asrtc::stream_schemas{ asrtc_stream_server_take( &asm_.stream ) };
        }

        asrtl_flat_tree const* collect_tree()
        {
                return asrtc_collect_server_tree( &asm_.collect );
        }

        asrtc_assembly& assembly()
        {
                return asm_;
        }

        friend task< void > async_destroy( task_ctx&, cntr_tcp_sys& );

private:
        bool                        disconnected_ = false;
        uv_idle_t                   idle_handle;
        std::shared_ptr< uv_tcp_t > client;
        clock const&                clk_;
        asrtc_assembly              asm_;

        cobs_node rx;
};

inline task< void > async_destroy( task_ctx&, cntr_tcp_sys& sys )
{
        uv_idle_stop( &sys.idle_handle );
        co_await uv_close_handle{ (uv_handle_t*) &sys.idle_handle };
        if ( !sys.disconnected_ )
                co_await uv_close_handle{ (uv_handle_t*) sys.client.get() };
}


struct suite_reporter
{
        virtual void on_count( uint32_t total ) = 0;
        virtual void on_test_start(
            std::string_view name,
            uint32_t         run_idx,
            uint32_t         run_total ) = 0;
        virtual void on_test_done(
            std::string_view name,
            bool             passed,
            double           duration_ms,
            uint32_t         run_idx,
            uint32_t         run_total ) = 0;
        virtual void on_diagnostic(
            std::string_view file,
            uint32_t         line,
            std::string_view extra )                                                       = 0;
        virtual void on_collect_data( std::string_view name, asrtl_flat_tree const* tree ) = 0;
        virtual void on_stream_data(
            std::string_view             name,
            asrtc::stream_schemas const& schemas ) = 0;
        virtual ~suite_reporter()                  = default;
};

struct _cntr_assembly_exec_test
{
        using value_sig = ecor::set_value_t( asrtc::result );

        cntr_tcp_sys&             sys;
        uint16_t                  tid;
        asrtl_flat_tree const*    tree;
        asrtl_flat_id             root_id;
        std::chrono::milliseconds timeout;

        _cntr_assembly_exec_test(
            cntr_tcp_sys&             s,
            uint16_t                  tid_,
            asrtl_flat_tree const*    t,
            asrtl_flat_id             rid,
            std::chrono::milliseconds to )
          : sys( s )
          , tid( tid_ )
          , tree( t )
          , root_id( rid )
          , timeout( to )
        {
        }

        template < typename OP >
        void start( OP& op )
        {
                auto s = asrtc_assembly_exec_test(
                    &sys.assembly(),
                    tree,
                    root_id,
                    tid,
                    static_cast< uint32_t >( timeout.count() ),
                    +[]( void* p, asrtl_status s, asrtc_result* res ) -> asrtl_status {
                            auto* op_ = static_cast< OP* >( p );
                            if ( s != ASRTL_SUCCESS ) {
                                    ASRTL_ERR_LOG(
                                        "asrtio_main",
                                        "Assembly exec_test failed: %s",
                                        asrtl_status_to_str( s ) );
                                    op_->recv.set_error( status::query_failed );
                                    return ASRTL_SUCCESS;
                            }
                            op_->recv.set_value( *res );
                            return ASRTL_SUCCESS;
                    },
                    &op );
                if ( s != ASRTL_SUCCESS ) {
                        ASRTL_ERR_LOG(
                            "asrtio_main",
                            "Assembly exec_test failed: %s",
                            asrtl_status_to_str( s ) );
                        op.recv.set_error( status::query_failed );
                }
        }
};
using cntr_assembly_exec_test = asrtl::gen_sender< _cntr_assembly_exec_test, status >;

inline void write_strm_field( std::ostream& os, enum asrtl_strm_field_type_e ft, uint8_t*& p )
{
        switch ( ft ) {
        case ASRTL_STRM_FIELD_U8:
                os << static_cast< unsigned >( *p++ );
                break;
        case ASRTL_STRM_FIELD_U16: {
                uint16_t v;
                asrtl_cut_u16( &p, &v );
                os << v;
                break;
        }
        case ASRTL_STRM_FIELD_U32: {
                uint32_t v;
                asrtl_cut_u32( &p, &v );
                os << v;
                break;
        }
        case ASRTL_STRM_FIELD_I8:
                os << static_cast< int >( static_cast< int8_t >( *p++ ) );
                break;
        case ASRTL_STRM_FIELD_I16: {
                uint16_t uv;
                asrtl_cut_u16( &p, &uv );
                os << static_cast< int16_t >( uv );
                break;
        }
        case ASRTL_STRM_FIELD_I32: {
                int32_t v;
                asrtl_cut_i32( &p, &v );
                os << v;
                break;
        }
        case ASRTL_STRM_FIELD_FLOAT: {
                uint32_t bits;
                asrtl_cut_u32( &p, &bits );
                float v;
                std::memcpy( &v, &bits, 4 );
                os << v;
                break;
        }
        case ASRTL_STRM_FIELD_BOOL:
                os << ( *p++ ? "true" : "false" );
                break;
        default:
                break;
        }
}

inline void write_stream_csv(
    output_fs&                   fs,
    std::filesystem::path const& path,
    asrtc_stream_schema const&   sc )
{
        auto  w  = fs.open_write( path );
        auto& os = w.stream();
        for ( uint8_t fi = 0; fi < sc.field_count; ++fi ) {
                if ( fi > 0 )
                        os << ",";
                os << asrtl_strm_field_type_to_str( sc.fields[fi] );
        }
        os << "\n";
        for ( auto* rec = sc.first; rec; rec = rec->next ) {
                uint8_t* p = rec->data;
                for ( uint8_t fi = 0; fi < sc.field_count; ++fi ) {
                        if ( fi > 0 )
                                os << ",";
                        write_strm_field( os, sc.fields[fi], p );
                }
                os << "\n";
        }
}

inline void handle_stream(
    cntr_tcp_sys&                sys,
    suite_reporter&              reporter,
    std::string_view             name,
    output_fs&                   fs,
    std::filesystem::path const& run_dir,
    bool                         do_output )
{
        auto schemas = sys.stream_take();
        if ( schemas->schema_count == 0 )
                return;
        reporter.on_stream_data( name, schemas );
        if ( !do_output )
                return;
        auto const& ss = *schemas;
        for ( uint32_t si = 0; si < ss.schema_count; ++si ) {
                auto const& sc = ss.schemas[si];
                write_stream_csv(
                    fs, run_dir / ( "stream." + std::to_string( sc.schema_id ) + ".csv" ), sc );
        }
}

inline void handle_collect(
    cntr_tcp_sys&                sys,
    suite_reporter&              reporter,
    std::string_view             name,
    output_fs&                   fs,
    std::filesystem::path const& path,
    bool                         do_output )
{
        auto const* tree = sys.collect_tree();
        if ( !tree )
                return;
        reporter.on_collect_data( name, tree );
        if ( !do_output )
                return;
        nlohmann::json j;
        if ( flat_tree_to_json( const_cast< asrtl_flat_tree& >( *tree ), j ) ) {
                auto w = fs.open_write( path );
                w.stream() << j.dump( 2 ) << "\n";
        }
}

inline void handle_diag(
    cntr_tcp_sys&                sys,
    suite_reporter&              reporter,
    output_fs&                   fs,
    std::filesystem::path const& path,
    bool                         do_output )
{
        std::optional< file_writer > w;
        if ( do_output ) {
                w.emplace( fs.open_write( path ) );
                w->stream() << "file,line,extra\n";
        }
        while ( auto rec = sys.take_diag_record() ) {
                char const* extra = rec->extra ? rec->extra : "";
                reporter.on_diagnostic( rec->file, rec->line, extra );
                if ( w )
                        w->stream() << rec->file << "," << rec->line << "," << extra << "\n";
        }
}

inline task< void > run_test_suite(
    task_ctx&                 ctx,
    cntr_tcp_sys&             sys,
    suite_reporter&           reporter,
    std::chrono::milliseconds timeout,
    param_config const&       params,
    output_fs&                fs,
    std::filesystem::path     output_dir )
{
        co_await cntr_start{ sys.cntr(), timeout };

        uint32_t count = co_await cntr_query_test_count{ sys.cntr(), timeout };
        reporter.on_count( count );

        std::set< std::string > unseen_keys;
        for ( auto const& [key, _] : params.tests )
                unseen_keys.insert( key );

        for ( uint32_t i = 0; i < count; ++i ) {
                auto [tid, name] = co_await cntr_query_test_info{ sys.cntr(), i, timeout };

                unseen_keys.erase( name );

                auto [skip, roots] = params.runs_for( name );

                if ( skip )
                        continue;

                uint32_t const run_total = static_cast< uint32_t >( roots.size() );

                for ( uint32_t ri = 0; ri < run_total; ++ri ) {
                        reporter.on_test_start( name, ri + 1, run_total );

                        auto          t0  = sys.clk().now();
                        asrtc::result res = co_await cntr_assembly_exec_test{
                            sys, tid, roots[ri] != 0 ? &params.tree : nullptr, roots[ri], timeout };

                        bool const do_output = !output_dir.empty();
                        auto const run_dir   = output_dir / name / std::to_string( ri );

                        if ( do_output )
                                fs.create_directories( run_dir );
                        handle_diag( sys, reporter, fs, run_dir / "diag.csv", do_output );
                        handle_collect(
                            sys, reporter, name, fs, run_dir / "collect.json", do_output );
                        handle_stream( sys, reporter, name, fs, run_dir, do_output );

                        double ms     = static_cast< double >( ( sys.clk().now() - t0 ).count() );
                        bool   passed = ( res.res == ASRTC_TEST_SUCCESS );
                        reporter.on_test_done( name, passed, ms, ri + 1, run_total );
                }
        }

        for ( auto const& key : unseen_keys )
                ASRTL_INF_LOG(
                    "asrtio",
                    "param config: key \"%s\" does not match any device test",
                    key.c_str() );

        co_return;
}


}  // namespace asrtio
