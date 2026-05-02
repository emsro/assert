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
#include "../asrtlpp/util.hpp"

namespace asrt
{

using status = asrt_status;

/// Owning handle for a collection of stream schemas and their records.
///
/// Returned by stream_server::take().  Movable, not copyable.  Frees all
/// schemas, records, and the backing array on destruction.
struct stream_schemas
{
        stream_schemas() = default;

        explicit stream_schemas( asrt_stream_schemas s )
          : _s( s )
        {
        }

        stream_schemas( stream_schemas const& )            = delete;
        stream_schemas& operator=( stream_schemas const& ) = delete;

        stream_schemas( stream_schemas&& o ) noexcept
          : _s( o._s )
        {
                o._s = {};
        }

        stream_schemas& operator=( stream_schemas&& o ) noexcept;

        ~stream_schemas();

        asrt_stream_schemas const* operator->() const { return &_s; }

        asrt_stream_schemas const& operator*() const { return _s; }

private:
        asrt_stream_schemas _s{};
};

inline status init( ref< asrt_stream_server > srv, asrt_node& prev, asrt_allocator alloc )
{
        return asrt_stream_server_init( srv, &prev, alloc );
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
