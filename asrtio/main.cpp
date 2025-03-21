
#include "../asrtcpp/controller.hpp"
#include "./deps/oof.h"

#include <CLI/CLI.hpp>
#include <boost/asio.hpp>
#include <boost/process/v2/process.hpp>
#include <boost/process/v2/stdio.hpp>

namespace asio    = boost::asio;
namespace process = boost::process;
using asrtc::opt;

int main( int argc, char* argv[] )
{

        CLI::App app{ "App description" };
        argv = app.ensure_utf8( argv );

        opt< std::filesystem::path > test_binary;
        app.add_option( "-e", test_binary, "Test binary to execute" );

        CLI11_PARSE( app, argc, argv );

        assert( test_binary );

        asio::io_context    ctx;
        asio::readable_pipe rp{ ctx };
        asio::writable_pipe wp{ ctx };

        process::process proc(
            ctx, test_binary->string(), {}, process::process_stdio{ wp, rp, {} } );


        auto cntr = asrtc::make_controller(
            [&]( asrtl::chann_id, std::span< std::byte > ) -> asrtl::status {
                    return ASRTL_SUCCESS;
            },
            [&]( asrtc::source, asrtl::ecode ) -> asrtc::status {
                    return ASRTC_SUCCESS;
            } );


        return 0;
}
