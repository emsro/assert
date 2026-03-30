#pragma once

#include "./util.hpp"

#include <concepts>

namespace asrtl
{

template < typename CB >
concept sender_callable = requires( CB cb, chann_id id, rec_span* buff ) {
        { cb( id, buff ) } -> std::same_as< status >;
};

template < typename CB >
inline status sender_cb( void* data, chann_id id, rec_span* buff )
{
        auto* cb = reinterpret_cast< CB* >( data );
        return ( *cb )( id, buff );
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
