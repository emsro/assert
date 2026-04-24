#pragma once

#include "./util.hpp"

#include <concepts>

namespace asrt
{

template < typename CB >
concept sender_callable =
    requires( CB cb, chann_id id, rec_span* buff, asrtl_send_done_cb dcb, void* dptr ) {
            { cb( id, buff, dcb, dptr ) } -> std::same_as< status >;
    };

template < typename CB >
inline status sender_cb(
    void*              data,
    chann_id           id,
    rec_span*          buff,
    asrtl_send_done_cb done_cb,
    void*              done_ptr )
{
        auto* cb = reinterpret_cast< CB* >( data );
        return ( *cb )( id, buff, done_cb, done_ptr );
}

using sender = asrtl_sender;

struct autosender
{
        template < sender_callable CB >
        autosender( CB& cb ) noexcept
          : _s{ .ptr = &cb, .cb = &sender_cb< CB > }
        {
        }

        autosender( asrtl_sender s ) noexcept
          : _s( s )
        {
        }

        operator asrtl_sender() const noexcept { return _s; }

private:
        asrtl_sender _s;
};

}  // namespace asrt
