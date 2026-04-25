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

#include "./util.hpp"

#include <concepts>

namespace asrt
{

template < typename CB >
concept sender_callable =
    requires( CB cb, chann_id id, rec_span* buff, asrt_send_done_cb dcb, void* dptr ) {
            { cb( id, buff, dcb, dptr ) } -> std::same_as< status >;
    };

template < typename CB >
inline status sender_cb(
    void*             data,
    chann_id          id,
    rec_span*         buff,
    asrt_send_done_cb done_cb,
    void*             done_ptr )
{
        auto* cb = reinterpret_cast< CB* >( data );
        return ( *cb )( id, buff, done_cb, done_ptr );
}

using sender = asrt_sender;

struct autosender
{
        template < sender_callable CB >
        autosender( CB& cb ) noexcept
          : _s{ .ptr = &cb, .cb = &sender_cb< CB > }
        {
        }

        autosender( asrt_sender s ) noexcept
          : _s( s )
        {
        }

        operator asrt_sender() const noexcept { return _s; }

private:
        asrt_sender _s;
};

}  // namespace asrt
