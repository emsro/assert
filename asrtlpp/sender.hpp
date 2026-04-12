#pragma once

#include "./util.hpp"

#include <concepts>

namespace asrtl
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

template < sender_callable CB >
inline asrtl_sender make_sender( CB& cb )
{
        return {
            .ptr = &cb,
            .cb  = &sender_cb< CB >,
        };
}
}  // namespace asrtl
