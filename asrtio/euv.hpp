#pragma once

#include "../asrtcpp/controller.hpp"
#include "./task.hpp"

#include <chrono>
#include <ecor/ecor.hpp>
#include <uv.h>

namespace asrtio
{


template < typename T >
struct _sender
{
        using sender_concept = ecor::sender_t;
        using context_type   = T;
        using value_sig      = typename T::value_sig;

        T ctx;

        template < typename... Args >
        _sender( Args&&... args )
          : ctx( (Args&&) args... )
        {
        }

        using completion_signatures =
            ecor::completion_signatures< value_sig, ecor::set_error_t( status ) >;


        template < typename R >
        struct _op
        {
                R            recv;
                context_type ctx;

                void start()
                {
                        ctx.start( *this );
                }
        };

        template < typename R >
        _op< R > connect( R&& receiver ) && noexcept
        {
                return { std::move( receiver ), std::move( ctx ) };
        }
};

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

using tcp_connect = _sender< _tcp_connect >;

struct _cntr_start
{
        using value_sig = ecor::set_value_t();

        asrtc::controller&    cntr;
        std::chrono::milliseconds timeout;

        _cntr_start( asrtc::controller& c, std::chrono::milliseconds timeout )
          : cntr( c )
          , timeout( timeout )
        {
        }

        template < typename OP >
        void start( OP& op )
        {
                auto x = cntr.start( [&op]( asrtc::status s ) {
                        if ( s != ASRTC_SUCCESS )
                                op.recv.set_error( status::init_failed );
                        else
                                op.recv.set_value();
                        return s;
                }, static_cast< uint32_t >( timeout.count() ) );
                if ( x != ASRTC_SUCCESS )
                        ASRTL_ERR_LOG(
                            "asrtio_main",
                            "Controller start failed: %s",
                            asrtc_status_to_str( x ) );
        }
};
using cntr_start = _sender< _cntr_start >;

struct _cntr_query_test_count
{
        using value_sig = ecor::set_value_t( uint32_t );

        asrtc::controller&    cntr;
        std::chrono::milliseconds timeout;

        _cntr_query_test_count( asrtc::controller& c, std::chrono::milliseconds timeout )
          : cntr( c )
          , timeout( timeout )
        {
        }

        template < typename OP >
        void start( OP& op )
        {
                auto s = cntr.query_test_count( [&op]( asrtc::status s, uint32_t count ) {
                        if ( s != ASRTC_SUCCESS ) {
                                op.recv.set_error( status::query_failed );
                                return ASRTC_SUCCESS;
                        }
                        op.recv.set_value( count );
                        return ASRTC_SUCCESS;
                }, static_cast< uint32_t >( timeout.count() ) );
                if ( s != ASRTC_SUCCESS ) {
                        op.recv.set_error( status::query_failed );
                        return;
                }
        }
};
using cntr_query_test_count = _sender< _cntr_query_test_count >;

struct _cntr_query_test_info
{
        using value_sig = ecor::set_value_t( uint16_t, std::string );

        asrtc::controller&    cntr;
        uint32_t              id;
        std::chrono::milliseconds timeout;

        _cntr_query_test_info( asrtc::controller& c, uint32_t id, std::chrono::milliseconds timeout )
          : cntr( c )
          , id( id )
          , timeout( timeout )
        {
        }

        template < typename OP >
        void start( OP& op )
        {
                auto s = cntr.query_test_info(
                    id, [&op]( asrtc::status s, uint16_t tid, std::string_view desc ) {
                            if ( s != ASRTC_SUCCESS ) {
                                    op.recv.set_error( status::query_failed );
                                    return ASRTC_SUCCESS;
                            }
                            op.recv.set_value( tid, std::string( desc ) );
                            return ASRTC_SUCCESS;
                    }, static_cast< uint32_t >( timeout.count() ) );
                if ( s != ASRTC_SUCCESS ) {
                        op.recv.set_error( status::query_failed );
                        return;
                }
        }
};
using cntr_query_test_info = _sender< _cntr_query_test_info >;

struct _cntr_exec_test
{
        using value_sig = ecor::set_value_t( asrtc::result );

        asrtc::controller&    cntr;
        uint32_t              id;
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
                auto s = cntr.exec_test( id, [&op]( asrtc::status s, asrtc::result const& res ) {
                        if ( s != ASRTC_SUCCESS ) {
                                op.recv.set_error( status::query_failed );
                                return ASRTC_SUCCESS;
                        }
                        op.recv.set_value( res );
                        return ASRTC_SUCCESS;
                }, static_cast< uint32_t >( timeout.count() ) );
                if ( s != ASRTC_SUCCESS ) {
                        op.recv.set_error( status::query_failed );
                        return;
                }
        }
};
using cntr_exec_test = _sender< _cntr_exec_test >;

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
using uv_close_handle = _sender< _uv_close >;

}  // namespace asrtio
