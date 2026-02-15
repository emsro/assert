#pragma once

#include "./util.hpp"

namespace asrtl
{

template < typename CB >
inline status sender_cb( void* data, chann_id id, span buff )
{
        auto*                cb = reinterpret_cast< CB* >( data );
        std::span< uint8_t > sp = cnv( buff );
        return ( *cb )( id, sp );
}

template < typename CB >
inline asrtl_sender make_sender( CB& cb )
{
        return {
            .ptr = &cb,
            .cb  = &sender_cb< CB >,
        };
}
}  // namespace asrtl
