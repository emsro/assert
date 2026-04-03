#pragma once

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
                        op.recv.set_error( status::connect_failed );
                        return;
                }
                req.data = &op;
                auto res = uv_tcp_connect(
                    &req,
                    client,
                    (const struct sockaddr*) &dest,
                    []( uv_connect_t* req, int status ) {
                            auto& op = *static_cast< OP* >( req->data );
                            if ( status < 0 )
                                    op.recv.set_error( status::connect_failed );
                            else
                                    op.recv.set_value();
                    } );
                if ( res != 0 ) {
                        op.recv.set_error( status::connect_failed );
                        return;
                }
        }
};

using tcp_connect = asrtl::gen_sender< _tcp_connect, status >;

struct _cntr_start
{
        using value_sig = ecor::set_value_t();

        asrtc::controller&        cntr;
        std::chrono::milliseconds timeout;

        _cntr_start( asrtc::controller& c, std::chrono::milliseconds timeout )
          : cntr( c )
          , timeout( timeout )
        {
        }

        template < typename OP >
        void start( OP& op )
        {
                auto x = cntr.start(
                    [&op]( asrtc::status s ) {
                            if ( s != ASRTC_SUCCESS )
                                    op.recv.set_error( status::init_failed );
                            else
                                    op.recv.set_value();
                            return s;
                    },
                    static_cast< uint32_t >( timeout.count() ) );
                if ( x != ASRTC_SUCCESS )
                        ASRTL_ERR_LOG(
                            "asrtio_main",
                            "Controller start failed: %s",
                            asrtc_status_to_str( x ) );
        }
};
using cntr_start = asrtl::gen_sender< _cntr_start, status >;

struct _cntr_query_test_count
{
        using value_sig = ecor::set_value_t( uint32_t );

        asrtc::controller&        cntr;
        std::chrono::milliseconds timeout;

        _cntr_query_test_count( asrtc::controller& c, std::chrono::milliseconds timeout )
          : cntr( c )
          , timeout( timeout )
        {
        }

        template < typename OP >
        void start( OP& op )
        {
                auto s = cntr.query_test_count(
                    [&op]( asrtc::status s, uint32_t count ) {
                            if ( s != ASRTC_SUCCESS ) {
                                    op.recv.set_error( status::query_failed );
                                    return ASRTC_SUCCESS;
                            }
                            op.recv.set_value( count );
                            return ASRTC_SUCCESS;
                    },
                    static_cast< uint32_t >( timeout.count() ) );
                if ( s != ASRTC_SUCCESS ) {
                        op.recv.set_error( status::query_failed );
                        return;
                }
        }
};
using cntr_query_test_count = asrtl::gen_sender< _cntr_query_test_count, status >;

struct _cntr_query_test_info
{
        using value_sig = ecor::set_value_t( uint16_t, std::string );

        asrtc::controller&        cntr;
        uint32_t                  id;
        std::chrono::milliseconds timeout;

        _cntr_query_test_info(
            asrtc::controller&        c,
            uint32_t                  id,
            std::chrono::milliseconds timeout )
          : cntr( c )
          , id( id )
          , timeout( timeout )
        {
        }

        template < typename OP >
        void start( OP& op )
        {
                auto s = cntr.query_test_info(
                    id,
                    [&op]( asrtc::status s, uint16_t tid, std::string_view desc ) {
                            if ( s != ASRTC_SUCCESS ) {
                                    op.recv.set_error( status::query_failed );
                                    return ASRTC_SUCCESS;
                            }
                            op.recv.set_value( tid, std::string( desc ) );
                            return ASRTC_SUCCESS;
                    },
                    static_cast< uint32_t >( timeout.count() ) );
                if ( s != ASRTC_SUCCESS ) {
                        op.recv.set_error( status::query_failed );
                        return;
                }
        }
};
using cntr_query_test_info = asrtl::gen_sender< _cntr_query_test_info, status >;

struct _cntr_exec_test
{
        using value_sig = ecor::set_value_t( asrtc::result );

        asrtc::controller&        cntr;
        uint32_t                  id;
        std::chrono::milliseconds timeout;

        _cntr_exec_test( asrtc::controller& c, uint32_t id, std::chrono::milliseconds timeout )
          : cntr( c )
          , id( id )
          , timeout( timeout )
        {
        }

        template < typename OP >
        void start( OP& op )
        {
                auto s = cntr.exec_test(
                    id,
                    [&op]( asrtc::status s, asrtc::result const& res ) {
                            if ( s != ASRTC_SUCCESS ) {
                                    op.recv.set_error( status::query_failed );
                                    return ASRTC_SUCCESS;
                            }
                            op.recv.set_value( res );
                            return ASRTC_SUCCESS;
                    },
                    static_cast< uint32_t >( timeout.count() ) );
                if ( s != ASRTC_SUCCESS ) {
                        op.recv.set_error( status::query_failed );
                        return;
                }
        }
};
using cntr_exec_test = asrtl::gen_sender< _cntr_exec_test, status >;

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
using uv_close_handle = asrtl::gen_sender< _uv_close, status >;

}  // namespace asrtio
