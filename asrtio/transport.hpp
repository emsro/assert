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

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <uv.h>

namespace asrtio
{

// ---------------------------------------------------------------------------
// serial_config
// ---------------------------------------------------------------------------

struct serial_config
{
        std::string path;

        uint32_t baud = 115200;

        enum class parity_t : uint8_t
        {
                none,
                odd,
                even
        } parity = parity_t::none;

        enum class stop_bits_t : uint8_t
        {
                one,
                two
        } stop = stop_bits_t::one;

        enum class flow_t : uint8_t
        {
                none,
                rtscts,
                xonxoff
        } flow = flow_t::none;
};

// ---------------------------------------------------------------------------
// open_serial_port
//
// Opens the device at cfg.path and applies the requested line settings via
// POSIX termios (Linux / macOS).  The fd is set O_NONBLOCK so libuv can
// manage it with uv_pipe_open.
//
// Returns a valid fd (>= 0) on success.
// Returns -1 on failure; errno is set and errmsg is populated with a
// human-readable description.
// ---------------------------------------------------------------------------

#if defined( _WIN32 )
#error "open_serial_port: Windows serial (win_serial_transport) is not yet implemented"
#endif

int open_serial_port( serial_config const& cfg, std::string& errmsg );

// ---------------------------------------------------------------------------
// tcp_transport
//
// Wraps a connected uv_tcp_t. The caller is responsible for constructing
// the shared_ptr and completing the connection (e.g. via tcp_connect sender)
// before constructing this transport.
// ---------------------------------------------------------------------------

struct tcp_transport
{
        std::shared_ptr< uv_tcp_t > client;

        uv_stream_t* stream() { return reinterpret_cast< uv_stream_t* >( client.get() ); }

        void close() { uv_close( reinterpret_cast< uv_handle_t* >( client.get() ), nullptr ); }
};

// ---------------------------------------------------------------------------
// serial_transport
//
// Use the static open() factory to construct. Returns nullopt on failure
// with errmsg populated.  The underlying uv_pipe_t is heap-allocated so
// the transport is movable and the pipe address stays stable.
// ---------------------------------------------------------------------------

struct serial_transport
{
        std::shared_ptr< uv_pipe_t > pipe;

        // Factory function — no exceptions.  Returns nullopt on failure;
        // errmsg is populated with a human-readable description.
        static std::optional< serial_transport > open(
            uv_loop_t*           loop,
            serial_config const& cfg,
            std::string&         errmsg );

        uv_stream_t* stream() { return reinterpret_cast< uv_stream_t* >( pipe.get() ); }

        void close() { uv_close( reinterpret_cast< uv_handle_t* >( pipe.get() ), nullptr ); }
};

}  // namespace asrtio
