#include "../asrtl/chann.h"
#include "../asrtrpp/reactor.hpp"

#include <CLI/CLI.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <list>

namespace asio = boost::asio;
using tcp      = asio::ip::tcp;

void h_excp( std::exception_ptr e )
{
        try {
                if ( e )
                        std::rethrow_exception( e );
        }
        catch ( std::exception const& ex ) {
                std::cerr << "Exception: " << ex.what() << std::endl;
        }
}

asio::awaitable< void > handle_sock( tcp::socket sock );

asio::awaitable< void > listen( tcp::acceptor& acceptor )
{
        for ( ;; ) {
                co_spawn(
                    acceptor.get_executor(),
                    handle_sock( co_await acceptor.async_accept( asio::use_awaitable ) ),
                    h_excp );
        }
}

static constexpr std::size_t buff_offset = sizeof( asrtl_chann_id );

asio::awaitable< void > handle_sock( tcp::socket sock )
{
        std::list< std::vector< uint8_t > > buffers;

        auto cb = [&]( asrtl::chann_id id, std::span< std::byte > buff ) {
                auto* iter = (uint8_t*) std::prev( buff.data(), buff_offset );
                auto* p    = iter;
                asrtl_add_u16( &iter, id );
                auto* e = (uint8_t*) buff.data() + buff.size();

                auto& b = buffers.emplace_back( p - e, 0U );
                std::copy( p, e, b.data() );
                auto buffs_iter = --buffers.end();

                async_write(
                    sock,
                    asio::buffer( b ),
                    [buffs_iter, &buffers]( boost::system::error_code ec, std::size_t ) {
                            buffers.erase( buffs_iter );
                            if ( ec )
                                    std::cerr << "Error: " << ec.message() << std::endl;
                    } );
                return ASRTL_SUCCESS;
        };
        asrtr::reactor reac{ cb, "Test reactor" };

        while ( sock.is_open() ) {
                std::byte buff[128];
                // XXX this might break
                auto const n =
                    co_await sock.async_read_some( asio::buffer( buff ), asio::use_awaitable );

                // XXX: make C++ wrapper for asrtl_chann_dispatch
                asrtl_chann_dispatch(
                    reac.node(), asrtl_span{ .b = (uint8_t*) buff, .e = (uint8_t*) buff + n } );
        }
}

int main( int argc, char** argv )
{
        CLI::App app{ "Test reactor" };

        uint16_t port;
        app.add_option( "-p,--port", port, "Port to listen on" )->required();

        CLI11_PARSE( app, argc, argv );

        asio::io_context io_ctx;
        asio::signal_set signals{ io_ctx, SIGINT, SIGTERM };
        signals.async_wait( std::bind( &boost::asio::io_service::stop, &io_ctx ) );

        tcp::acceptor acceptor{ io_ctx, tcp::endpoint{ tcp::v4(), port } };

        if ( !acceptor.is_open() ) {
                std::cerr << "Failed to bind to port" << std::endl;
                return 1;
        }

        asio::co_spawn( io_ctx, listen( acceptor ), h_excp );

        io_ctx.run();

        return 0;
}
