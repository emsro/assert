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

        uv_tcp_t*        client;
        std::string_view host;
        uint16_t         port;
        sockaddr_in      dest;
        uv_connect_t     req;

        _tcp_connect( uv_tcp_t* client, std::string_view host, uint16_t port )
          : client( client )
          , host( host )
          , port( port )
        {
        }

        template < typename OP >
        void start( OP& op )
        {
                if ( uv_ip4_addr( host.data(), port, &dest ) != 0 ) {
                        ASRTL_ERR_LOG( "asrtio_main", "Failed to resolve address" );
                        op.recv.set_error( ASRTL_INIT_ERR );
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
                                    ASRTL_ERR_LOG(
                                        "asrtio_main",
                                        "TCP connect failed: %s",
                                        uv_strerror( status ) );
                                    op.recv.set_error( ASRTL_INIT_ERR );
                            } else {
                                    op.recv.set_value();
                            }
                    } );
                if ( res != 0 ) {
                        ASRTL_ERR_LOG(
                            "asrtio_main", "TCP connect request failed: %s", uv_strerror( res ) );
                        op.recv.set_error( ASRTL_INIT_ERR );
                        return;
                }
        }
};

using tcp_connect = asrt::gen_sender< _tcp_connect >;

struct _cntr_start
{
        using value_sig = ecor::set_value_t();

        asrtc_controller&         cntr;
        std::chrono::milliseconds timeout;

        _cntr_start( asrtc_controller& c, std::chrono::milliseconds timeout )
          : cntr( c )
          , timeout( timeout )
        {
        }

        template < typename OP >
        void start( OP& op )
        {
                auto s = asrtc_cntr_start(
                    &cntr,
                    +[]( void* p, asrtl_status s ) -> asrtl_status {
                            auto* op_ = static_cast< OP* >( p );
                            if ( s != ASRTL_SUCCESS ) {
                                    ASRTL_ERR_LOG(
                                        "asrtio_main",
                                        "Controller start callback failed: %s",
                                        asrtl_status_to_str( s ) );
                                    op_->recv.set_error( s );
                                    return ASRTL_SUCCESS;
                            }
                            op_->recv.set_value();
                            return ASRTL_SUCCESS;
                    },
                    &op,
                    static_cast< uint32_t >( timeout.count() ) );
                if ( s != ASRTL_SUCCESS ) {
                        ASRTL_ERR_LOG(
                            "asrtio_main",
                            "Controller start failed: %s",
                            asrtl_status_to_str( s ) );
                        op.recv.set_error( s );
                }
        }
};
using cntr_start = asrt::gen_sender< _cntr_start >;

struct _cntr_query_test_count
{
        using value_sig = ecor::set_value_t( uint32_t );

        asrtc_controller&         cntr;
        std::chrono::milliseconds timeout;

        _cntr_query_test_count( asrtc_controller& c, std::chrono::milliseconds timeout )
          : cntr( c )
          , timeout( timeout )
        {
        }

        template < typename OP >
        void start( OP& op )
        {
                auto s = asrtc_cntr_test_count(
                    &cntr,
                    +[]( void* p, asrtl_status s, uint16_t count ) -> asrtl_status {
                            auto* op_ = static_cast< OP* >( p );
                            if ( s != ASRTL_SUCCESS ) {
                                    ASRTL_ERR_LOG(
                                        "asrtio_main",
                                        "Query test count failed: %s",
                                        asrtl_status_to_str( s ) );
                                    op_->recv.set_error( s );
                                    return ASRTL_SUCCESS;
                            }
                            op_->recv.set_value( count );
                            return ASRTL_SUCCESS;
                    },
                    &op,
                    static_cast< uint32_t >( timeout.count() ) );
                if ( s != ASRTL_SUCCESS ) {
                        ASRTL_ERR_LOG(
                            "asrtio_main",
                            "Query test count failed: %s",
                            asrtl_status_to_str( s ) );
                        op.recv.set_error( s );
                }
        }
};
using cntr_query_test_count = asrt::gen_sender< _cntr_query_test_count >;

struct _cntr_query_test_info
{
        using value_sig = ecor::set_value_t( uint16_t, std::string );

        asrtc_controller&         cntr;
        uint32_t                  id;
        std::chrono::milliseconds timeout;

        _cntr_query_test_info( asrtc_controller& c, uint32_t id, std::chrono::milliseconds timeout )
          : cntr( c )
          , id( id )
          , timeout( timeout )
        {
        }

        template < typename OP >
        void start( OP& op )
        {
                auto s = asrtc_cntr_test_info(
                    &cntr,
                    static_cast< uint16_t >( id ),
                    +[]( void* p, asrtl_status s, uint16_t tid, char* desc ) -> asrtl_status {
                            auto* op_ = static_cast< OP* >( p );
                            if ( s != ASRTL_SUCCESS ) {
                                    ASRTL_ERR_LOG(
                                        "asrtio_main",
                                        "Query test info failed: %s",
                                        asrtl_status_to_str( s ) );
                                    op_->recv.set_error( s );
                                    return ASRTL_SUCCESS;
                            }
                            op_->recv.set_value( tid, std::string{ desc } );
                            return ASRTL_SUCCESS;
                    },
                    &op,
                    static_cast< uint32_t >( timeout.count() ) );
                if ( s != ASRTL_SUCCESS ) {
                        ASRTL_ERR_LOG(
                            "asrtio_main", "Query test info failed: %s", asrtl_status_to_str( s ) );
                        op.recv.set_error( s );
                }
        }
};
using cntr_query_test_info = asrt::gen_sender< _cntr_query_test_info >;

struct _cntr_exec_test
{
        using value_sig = ecor::set_value_t( asrt::result );

        asrtc_controller&         cntr;
        uint32_t                  id;
        std::chrono::milliseconds timeout;

        _cntr_exec_test( asrtc_controller& c, uint32_t id, std::chrono::milliseconds timeout )
          : cntr( c )
          , id( id )
          , timeout( timeout )
        {
        }

        template < typename OP >
        void start( OP& op )
        {
                auto s = asrtc_cntr_test_exec(
                    &cntr,
                    static_cast< uint16_t >( id ),
                    +[]( void* p, asrtl_status s, asrtc_result* res ) -> asrtl_status {
                            auto* op_ = static_cast< OP* >( p );
                            if ( s != ASRTL_SUCCESS ) {
                                    ASRTL_ERR_LOG(
                                        "asrtio_main",
                                        "Test execution failed: %s",
                                        asrtl_status_to_str( s ) );
                                    op_->recv.set_error( s );
                                    return ASRTL_SUCCESS;
                            }
                            op_->recv.set_value( *res );
                            return ASRTL_SUCCESS;
                    },
                    &op,
                    static_cast< uint32_t >( timeout.count() ) );
                if ( s != ASRTL_SUCCESS ) {
                        ASRTL_ERR_LOG(
                            "asrtio_main", "Test execution failed: %s", asrtl_status_to_str( s ) );
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
