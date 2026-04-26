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
#pragma once

#include "../asrtc/controller.h"
#include "../asrtcpp/controller.hpp"
#include "../asrtlpp/task.hpp"
#include "./task.hpp"

#include <chrono>
#include <uv.h>

namespace asrtio
{


struct _tcp_connect
{
        using value_sig = ecor::set_value_t();

        uv_tcp_t*    client;
        char const*  host;
        uint16_t     port;
        sockaddr_in  dest;
        uv_connect_t req;

        _tcp_connect( uv_tcp_t* client, char const* host, uint16_t port )
          : client( client )
          , host( host )
          , port( port )
        {
        }

        template < typename OP >
        void start( OP& op )
        {
                if ( uv_ip4_addr( host, port, &dest ) != 0 ) {
                        ASRT_ERR_LOG( "asrtio_main", "Failed to resolve address" );
                        op.recv.set_error( ASRT_INIT_ERR );
                        return;
                }
                req.data = &op;
                auto res = uv_tcp_connect(
                    &req,
                    client,
                    (const struct sockaddr*) &dest,
                    []( uv_connect_t* req, int status ) {
                            auto& op = *static_cast< OP* >( req->data );
                            if ( status < 0 ) {
                                    ASRT_ERR_LOG(
                                        "asrtio_main",
                                        "TCP connect failed: %s",
                                        uv_strerror( status ) );
                                    op.recv.set_error( ASRT_INIT_ERR );
                            } else {
                                    op.recv.set_value();
                            }
                    } );
                if ( res != 0 ) {
                        ASRT_ERR_LOG(
                            "asrtio_main", "TCP connect request failed: %s", uv_strerror( res ) );
                        op.recv.set_error( ASRT_INIT_ERR );
                        return;
                }
        }
};

using tcp_connect = asrt::gen_sender< _tcp_connect >;

struct _cntr_start
{
        using value_sig = ecor::set_value_t();

        asrt_controller&          cntr;
        std::chrono::milliseconds timeout;

        _cntr_start( asrt_controller& c, std::chrono::milliseconds timeout )
          : cntr( c )
          , timeout( timeout )
        {
        }

        template < typename OP >
        void start( OP& op )
        {
                auto s = asrt_cntr_start(
                    &cntr,
                    +[]( void* p, asrt_status s ) -> asrt_status {
                            auto* op = static_cast< OP* >( p );
                            if ( s != ASRT_SUCCESS ) {
                                    ASRT_ERR_LOG(
                                        "asrtio_main",
                                        "Controller start callback failed: %s",
                                        asrt_status_to_str( s ) );
                                    op->recv.set_error( s );
                                    return ASRT_SUCCESS;
                            }
                            op->recv.set_value();
                            return ASRT_SUCCESS;
                    },
                    &op,
                    static_cast< uint32_t >( timeout.count() ) );
                if ( s != ASRT_SUCCESS ) {
                        ASRT_ERR_LOG(
                            "asrtio_main", "Controller start failed: %s", asrt_status_to_str( s ) );
                        op.recv.set_error( s );
                }
        }
};
using cntr_start = asrt::gen_sender< _cntr_start >;

struct _cntr_query_test_count
{
        using value_sig = ecor::set_value_t( uint32_t );

        asrt_controller&          cntr;
        std::chrono::milliseconds timeout;

        _cntr_query_test_count( asrt_controller& c, std::chrono::milliseconds timeout )
          : cntr( c )
          , timeout( timeout )
        {
        }

        template < typename OP >
        void start( OP& op )
        {
                auto s = asrt_cntr_test_count(
                    &cntr,
                    +[]( void* p, asrt_status s, uint16_t count ) -> asrt_status {
                            auto* op = static_cast< OP* >( p );
                            if ( s != ASRT_SUCCESS ) {
                                    ASRT_ERR_LOG(
                                        "asrtio_main",
                                        "Query test count failed: %s",
                                        asrt_status_to_str( s ) );
                                    op->recv.set_error( s );
                                    return ASRT_SUCCESS;
                            }
                            op->recv.set_value( count );
                            return ASRT_SUCCESS;
                    },
                    &op,
                    static_cast< uint32_t >( timeout.count() ) );
                if ( s != ASRT_SUCCESS ) {
                        ASRT_ERR_LOG(
                            "asrtio_main", "Query test count failed: %s", asrt_status_to_str( s ) );
                        op.recv.set_error( s );
                }
        }
};
using cntr_query_test_count = asrt::gen_sender< _cntr_query_test_count >;

struct _cntr_query_test_info
{
        using value_sig = ecor::set_value_t( uint16_t, std::string );

        asrt_controller&          cntr;
        uint32_t                  id;
        std::chrono::milliseconds timeout;

        _cntr_query_test_info( asrt_controller& c, uint32_t id, std::chrono::milliseconds timeout )
          : cntr( c )
          , id( id )
          , timeout( timeout )
        {
        }

        template < typename OP >
        void start( OP& op )
        {
                auto s = asrt_cntr_test_info(
                    &cntr,
                    static_cast< uint16_t >( id ),
                    +[]( void* p, asrt_status s, uint16_t tid, char const* desc ) -> asrt_status {
                            auto* op = static_cast< OP* >( p );
                            if ( s != ASRT_SUCCESS ) {
                                    ASRT_ERR_LOG(
                                        "asrtio_main",
                                        "Query test info failed: %s",
                                        asrt_status_to_str( s ) );
                                    op->recv.set_error( s );
                                    return ASRT_SUCCESS;
                            }
                            op->recv.set_value( tid, std::string{ desc } );
                            return ASRT_SUCCESS;
                    },
                    &op,
                    static_cast< uint32_t >( timeout.count() ) );
                if ( s != ASRT_SUCCESS ) {
                        ASRT_ERR_LOG(
                            "asrtio_main", "Query test info failed: %s", asrt_status_to_str( s ) );
                        op.recv.set_error( s );
                }
        }
};
using cntr_query_test_info = asrt::gen_sender< _cntr_query_test_info >;

struct _cntr_exec_test
{
        using value_sig = ecor::set_value_t( asrt::result );

        asrt_controller&          cntr;
        uint32_t                  id;
        std::chrono::milliseconds timeout;

        _cntr_exec_test( asrt_controller& c, uint32_t id, std::chrono::milliseconds timeout )
          : cntr( c )
          , id( id )
          , timeout( timeout )
        {
        }

        template < typename OP >
        void start( OP& op )
        {
                auto s = asrt_cntr_test_exec(
                    &cntr,
                    static_cast< uint16_t >( id ),
                    +[]( void* p, asrt_status s, asrt_result* res ) -> asrt_status {
                            auto* op = static_cast< OP* >( p );
                            if ( s != ASRT_SUCCESS ) {
                                    ASRT_ERR_LOG(
                                        "asrtio_main",
                                        "Test execution failed: %s",
                                        asrt_status_to_str( s ) );
                                    op->recv.set_error( s );
                                    return ASRT_SUCCESS;
                            }
                            op->recv.set_value( *res );
                            return ASRT_SUCCESS;
                    },
                    &op,
                    static_cast< uint32_t >( timeout.count() ) );
                if ( s != ASRT_SUCCESS ) {
                        ASRT_ERR_LOG(
                            "asrtio_main", "Test execution failed: %s", asrt_status_to_str( s ) );
                        op.recv.set_error( s );
                }
        }
};
using cntr_exec_test = asrt::gen_sender< _cntr_exec_test >;

struct _uv_close
{
        using value_sig = ecor::set_value_t();

        uv_handle_t* handle;

        _uv_close( uv_handle_t* h )
          : handle( h )
        {
        }

        template < typename OP >
        void start( OP& op )
        {
                handle->data = &op;
                uv_close( handle, []( uv_handle_t* h ) {
                        auto& op = *static_cast< OP* >( h->data );
                        op.recv.set_value();
                } );
        }
};
using uv_close_handle = asrt::gen_sender< _uv_close >;

}  // namespace asrtio
