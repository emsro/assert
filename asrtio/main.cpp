
#include "../asrtcpp/controller.hpp"
#include "../asrtl/util.h"
#include "../asrtlpp/fmt.hpp"
#include "./deps/oof.h"

#include <CLI/CLI.hpp>
#include <boost/asio.hpp>
#include <list>

namespace asio = boost::asio;
using asrtl::opt;
using tcp = asio::ip::tcp;

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

struct tcp_opts
{
        std::string host;
        uint16_t    port;
};

asio::awaitable< void > run_tcp( asio::io_context& ctx, std::shared_ptr< tcp_opts > opts );

void setup_tcp_command( asio::io_context& ctx, CLI::App& app )
{
        auto  opt = std::make_shared< tcp_opts >();
        auto* sub = app.add_subcommand( "tcp", "Connect to TCP-based system" );

        sub->add_option( "-p,--port", opt->port, "Port to connect to" );
        sub->add_option( "-h,--host", opt->host, "Host to connect to" );

        sub->callback( [opt, &ctx] {
                asio::co_spawn( ctx, run_tcp( ctx, opt ), h_excp );
        } );
}

asio::awaitable< void > run_tcp( asio::io_context& ctx, std::shared_ptr< tcp_opts > opts )
{
        tcp::resolver resolver( ctx );

        tcp::resolver::results_type endpoints =
            resolver.resolve( opts->host, std::to_string( opts->port ) );

        if ( endpoints.empty() ) {
                std::cerr << "No endpoints found" << std::endl;
                co_return;
        }
        tcp::socket sock( ctx );
        co_await sock.async_connect( endpoints.begin()->endpoint(), asio::use_awaitable );

        std::list< std::vector< uint8_t > > buffers;

        asrtc::controller cntr{
            [&]( asrtl::chann_id chid, std::span< std::byte > b ) -> asrtl::status {
                    buffers.emplace_back( b.size() + sizeof( chid ), 0U );
                    auto  biter = --buffers.end();
                    auto* p     = biter->data();
                    asrtl_add_u16( &p, chid );
                    std::copy_n( (uint8_t*) b.data(), b.size(), p );

                    async_write(
                        sock,
                        asio::buffer( *biter ),
                        [biter, &buffers]( boost::system::error_code ec, std::size_t ) {
                                buffers.erase( biter );
                                if ( ec )
                                        std::cerr << "Error: " << ec.message() << std::endl;
                        } );
                    return ASRTL_SUCCESS;
            },
            [&]( asrtl::source sr, asrtl::ecode ec ) -> asrtc::status {
                    std::cerr << std::format( "({}) Error: {} ", sr, ec ) << std::endl;
                    std::abort();  // XXX: improve
            } };

        while ( sock.is_open() ) {
                std::byte buff[128];
                // XXX this might break
                auto const n =
                    co_await sock.async_read_some( asio::buffer( buff ), asio::use_awaitable );

                // XXX: make C++ wrapper for asrtl_chann_dispatch
                asrtl_chann_dispatch(
                    cntr.node(), asrtl_span{ .b = (uint8_t*) buff, .e = (uint8_t*) buff + n } );
        }

        co_return;
}

int main( int argc, char* argv[] )
{
        asio::io_context ctx;
        CLI::App         app{ "App description" };
        argv = app.ensure_utf8( argv );

        setup_tcp_command( ctx, app );

        CLI11_PARSE( app, argc, argv );


        return 0;
}
