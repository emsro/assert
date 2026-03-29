#pragma once

#include "../asrtc/status_to_str.h"
#include "../asrtcpp/controller.hpp"
#include "../asrtcpp/diag.hpp"
#include "../asrtcpp/param.hpp"
#include "../asrtl/log.h"
#include "../asrtlpp/fmt.hpp"
#include "../asrtlpp/util.hpp"
#include "./euv.hpp"
#include "./param_config.hpp"
#include "./task.hpp"
#include "./util.hpp"

#include <chrono>
#include <set>
#include <uv.h>

namespace asrtio
{

struct cntr_tcp_sys
{

        cntr_tcp_sys( uv_tcp_t* client, clock const& clk )
          : client( client )
          , clk_( clk )
        {
        }

        auto take_diag_record()
        {
                return c_diag.take_record();
        }

        asrtc::param_server& param()
        {
                return c_param;
        }

        clock const& clk() const
        {
                return clk_;
        }

        template < typename CB >
        void set_param_tree(
            asrtl_flat_tree const*    tree,
            asrtl_flat_id             root_id,
            CB&                       on_ack,
            std::chrono::milliseconds timeout )
        {
                c_param.set_tree( tree );
                std::ignore = c_param.send_ready(
                    root_id, on_ack, static_cast< uint32_t >( timeout.count() ) );
        }

        void tick()
        {
                auto now = static_cast< uint32_t >( clk_.now().count() );
                if ( auto s = cntr.tick( now ); s != ASRTC_SUCCESS )
                        ASRTL_ERR_LOG(
                            "asrtio_main", "Controller tick failed: %s", asrtc_status_to_str( s ) );
                std::ignore = c_param.tick( now );
        }


        void start()
        {
                uv_idle_init( client->loop, &idle_handle );
                idle_handle.data = this;
                uv_idle_start( &idle_handle, []( uv_idle_t* h ) {
                        static_cast< cntr_tcp_sys* >( h->data )->tick();
                } );
                rx.start( (uv_stream_t*) client, cntr.node(), [this]( ssize_t nread ) {
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
                uv_idle_stop( &idle_handle );
                uv_close( (uv_handle_t*) &idle_handle, nullptr );
                uv_close( (uv_handle_t*) client, nullptr );
        }

        friend task< void > async_close( task_ctx&, cntr_tcp_sys& );

private:
        uv_idle_t                                                        idle_handle;
        uv_tcp_t*                                                        client;
        clock const&                                                     clk_;
        std::function< asrtl_status( asrtl_chann_id, asrtl_rec_span* ) > cntr_send{
            [this]( asrtl_chann_id id, asrtl_rec_span* buff ) -> asrtl_status {
                    return rx.write( (uv_stream_t*) client, id, *buff );
            } };

public:
        asrtc::controller cntr{
            cntr_send,
            [this]( asrtl::source sr, asrtl::ecode ec ) -> asrtc::status {
                    auto s = std::format( "Source: {}, code: {}", sr, ec );
                    ASRTL_ERR_LOG( "asrtio_main", "%s", s.c_str() );
                    close();
                    return ASRTC_SUCCESS;
            } };


        asrtc::diag         c_diag{ cntr.node(), cntr_send };
        asrtc::param_server c_param{ c_diag.node(), cntr_send, asrtl_default_allocator() };

        cobs_node rx;
};

inline task< void > async_close( task_ctx&, cntr_tcp_sys& sys )
{
        uv_idle_stop( &sys.idle_handle );
        co_await uv_close_handle{ (uv_handle_t*) &sys.idle_handle };
        co_await uv_close_handle{ (uv_handle_t*) sys.client };
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
            uint32_t         run_total )                                           = 0;
        virtual void on_diagnostic( std::string_view file, uint32_t line, std::string_view extra ) = 0;
        virtual ~suite_reporter()                                          = default;
};

struct _cntr_set_param_tree
{
        using value_sig = ecor::set_value_t();

        cntr_tcp_sys&             sys;
        asrtl_flat_tree const*    tree;
        asrtl_flat_id             root_id;
        std::chrono::milliseconds timeout;
        std::function< void() >   on_ack_;

        _cntr_set_param_tree(
            cntr_tcp_sys&             s,
            asrtl_flat_tree const*    t,
            asrtl_flat_id             rid,
            std::chrono::milliseconds timeout )
          : sys( s )
          , tree( t )
          , root_id( rid )
          , timeout( timeout )
        {
        }

        template < typename OP >
        void start( OP& op )
        {
                on_ack_ = [&op] {
                        op.recv.set_value();
                };
                sys.set_param_tree( tree, root_id, on_ack_, timeout );
        }
};
using cntr_set_param_tree = _sender< _cntr_set_param_tree >;

inline task< void > run_test_suite(
    task_ctx&                 ctx,
    cntr_tcp_sys&             sys,
    suite_reporter&           reporter,
    std::chrono::milliseconds timeout,
    param_config const&       params )
{
        co_await cntr_start{ sys.cntr, timeout };

        uint32_t count = co_await cntr_query_test_count{ sys.cntr, timeout };
        reporter.on_count( count );

        std::set< std::string > unseen_keys;
        for ( auto const& [key, _] : params.tests )
                unseen_keys.insert( key );

        for ( uint32_t i = 0; i < count; ++i ) {
                auto [tid, name] = co_await cntr_query_test_info{ sys.cntr, i, timeout };

                unseen_keys.erase( name );

                auto [skip, roots] = params.runs_for( name );

                if ( skip )
                        continue;

                uint32_t const run_total = static_cast< uint32_t >( roots.size() );

                for ( uint32_t ri = 0; ri < run_total; ++ri ) {
                        if ( roots[ri] != 0 )
                                co_await cntr_set_param_tree{
                                    sys, &params.tree, roots[ri], timeout };

                        reporter.on_test_start( name, ri + 1, run_total );
                        auto          t0  = sys.clk().now();
                        asrtc::result res = co_await cntr_exec_test{ sys.cntr, tid, timeout };

                        while ( auto rec = sys.take_diag_record() )
                                reporter.on_diagnostic(
                                    rec->file, rec->line, rec->extra ? rec->extra : "" );
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
