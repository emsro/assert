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

#include "../asrtc/stream.h"
#include "../asrtl/asrt_assert.h"
#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"
#include "../asrtlpp/sender.hpp"

namespace asrt
{

/// Owning handle for a collection of stream schemas and their records.
///
/// Returned by stream_server::take().  Movable, not copyable.  Frees all
/// schemas, records, and the backing array on destruction.
struct stream_schemas
{
        stream_schemas() = default;

        explicit stream_schemas( asrt_stream_schemas s )
          : s_( s )
        {
        }

        stream_schemas( stream_schemas const& )            = delete;
        stream_schemas& operator=( stream_schemas const& ) = delete;

        stream_schemas( stream_schemas&& o ) noexcept
          : s_( o.s_ )
        {
                o.s_ = {};
        }

        stream_schemas& operator=( stream_schemas&& o ) noexcept
        {
                if ( this != &o ) {
                        asrt_stream_schemas_free( &s_ );
                        s_   = o.s_;
                        o.s_ = {};
                }
                return *this;
        }

        ~stream_schemas() { asrt_stream_schemas_free( &s_ ); }

        asrt_stream_schemas const* operator->() const { return &s_; }

        asrt_stream_schemas const& operator*() const { return s_; }

private:
        asrt_stream_schemas s_{};
};

inline status init(
    ref< asrt_stream_server > srv,
    asrt_node&                prev,
    autosender                send_cb,
    asrt_allocator            alloc )
{
        return asrt_stream_server_init( srv, &prev, send_cb, alloc );
}

inline stream_schemas take( ref< asrt_stream_server > srv )
{
        return stream_schemas{ asrt_stream_server_take( srv ) };
}

inline void clear( ref< asrt_stream_server > srv )
{
        asrt_stream_server_clear( srv );
}

inline void deinit( ref< asrt_stream_server > srv )
{
        asrt_stream_server_deinit( srv );
}

}  // namespace asrt
