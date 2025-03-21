#include "../asrtrpp/reactor.hpp"

#include <CLI/CLI.hpp>
#include <boost/asio.hpp>
#include <iostream>

namespace asio = boost::asio;

void handle_exception( std::exception_ptr e )
{
        try {
                if ( e )
                        std::rethrow_exception( e );
        }
        catch ( std::exception const& ex ) {
                std::cerr << "Exception: " << ex.what() << std::endl;
        }
}

static constexpr std::size_t buff_offset = sizeof( asrtl_chann_id );

asio::awaitable< void > ticker( asrtr::reactor& reac )
{
        asio::steady_timer timer( co_await asio::this_coro::executor );
        auto               period = std::chrono::milliseconds( 1 );
        for ( ;; ) {
                std::byte buff[64];
                std::span sp{ buff };
                reac.tick( sp.subspan( buff_offset ) );

                timer.expires_after( period );
                co_await timer.async_wait( asio::use_awaitable );
        }
}

asio::awaitable< void > reader( asio::ip::tcp::socket& socket, asrtr::reactor& reac )
{
        std::vector< std::byte > buff( 128 );
        for ( ;; ) {
                std::size_t n =
                    co_await socket.async_read_some( asio::buffer( buff ), asio::use_awaitable );
                buff.resize( n );

                std::span sp{ buff };
                asrtl_chann_dispatch( reac.node(), asrtr::cnv( sp ) );
        }
}

int main( int argc, char** argv )
{
        CLI::App app{ "Test reactor" };

        unsigned port;
        app.add_option( "-p,--port", port, "Port to listen on" )->required();

        CLI11_PARSE( app, argc, argv );

        asio::io_context ctx;
        asio::signal_set signals{ ctx, SIGINT, SIGTERM };
        signals.async_wait( std::bind( &boost::asio::io_service::stop, &ctx ) );

        asio::ip::tcp::socket socket( ctx, asio::ip::tcp::endpoint( asio::ip::tcp::v4(), port ) );

        auto cb = [&]( asrtl::chann_id id, std::span< std::byte > buff ) {
                auto* iter = (uint8_t*) std::prev( buff.data(), buff_offset );
                auto* p    = iter;
                asrtl_add_u16( &iter, id );

                socket.async_write_some(
                    asio::buffer( p, buff.size() + buff_offset ),
                    []( boost::system::error_code ec, std::size_t ) {
                            if ( ec )
                                    std::cerr << "Error: " << ec.message() << std::endl;
                    } );
                return ASRTL_SUCCESS;
        };
        asrtr::reactor reac{ cb, "Test reactor" };

        asio::co_spawn( ctx, ticker( reac ), handle_exception );
        asio::co_spawn( ctx, reader( socket, reac ), handle_exception );

        if ( !socket.is_open() ) {
                std::cerr << "Failed to open socket" << std::endl;
                return 1;
        }

        ctx.run();

        return 0;
}
