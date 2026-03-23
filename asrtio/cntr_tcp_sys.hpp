#pragma once

#include "../asrtc/status_to_str.h"
#include "../asrtcpp/controller.hpp"
#include "../asrtcpp/diag.hpp"
#include "../asrtcpp/param.hpp"
#include "../asrtl/log.h"
#include "../asrtlpp/fmt.hpp"
#include "../asrtlpp/util.hpp"
#include "./euv.hpp"
#include "./task.hpp"
#include "./util.hpp"

#include <chrono>
#include <uv.h>

namespace asrtio
{

struct cntr_tcp_sys
{

        cntr_tcp_sys( uv_tcp_t* client )
          : client( client )
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

        void set_param_tree( asrtl_flat_tree const* tree, asrtl_flat_id root_id )
        {
                c_param.set_tree( tree );
                std::ignore = c_param.send_ready( root_id );
        }

        void tick()
        {
                if ( auto s = cntr.tick(); s != ASRTC_SUCCESS )
                        ASRTL_ERR_LOG(
                            "asrtio_main", "Controller tick failed: %s", asrtc_status_to_str( s ) );
                std::ignore = c_param.tick();
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

private:
        uv_idle_t                                                        idle_handle;
        uv_tcp_t*                                                        client;
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
            },
            [this]( asrtc::status s ) -> asrtc::status {
                    // XXX: this should be awaited somewhere.
                    if ( s != ASRTC_SUCCESS )
                            ASRTL_ERR_LOG(
                                "asrtio_main",
                                "Controller init failed: %s",
                                asrtc_status_to_str( s ) );
                    return s;
            } };


        asrtc::diag         c_diag{ cntr.node(), cntr_send };
        asrtc::param_server c_param{ c_diag.node(), cntr_send, asrtl_default_allocator() };

        cobs_node rx;
};


struct suite_reporter
{
        virtual void on_count( uint32_t total )                                             = 0;
        virtual void on_test_start( std::string_view name )                                 = 0;
        virtual void on_test_done( std::string_view name, bool passed, double duration_ms ) = 0;
        virtual void on_diagnostic( std::string_view file, uint32_t line )                  = 0;
        virtual ~suite_reporter() = default;
};

inline task< void > run_test_suite( task_ctx& ctx, cntr_tcp_sys& sys, suite_reporter& reporter )
{
        while ( !sys.cntr.is_idle() )
                co_await ecor::suspend;

        uint32_t count = co_await cntr_query_test_count{ sys.cntr };
        reporter.on_count( count );

        for ( uint32_t i = 0; i < count; ++i ) {
                auto [tid, name] = co_await cntr_query_test_info{ sys.cntr, i };
                reporter.on_test_start( name );
                asrtc::result res = co_await cntr_exec_test{ sys.cntr, tid };

                while ( auto rec = sys.take_diag_record() )
                        reporter.on_diagnostic( rec->file, rec->line );
                using namespace std::chrono;
                double ms =
                    duration_cast< duration< double, std::milli > >(
                        std::chrono::steady_clock::now() - std::chrono::steady_clock::time_point{} )
                        .count();
                bool passed = ( res.res == ASRTC_TEST_SUCCESS );
                reporter.on_test_done( name, passed, ms );
        }

        co_return;
}


}  // namespace asrtio
