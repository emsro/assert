#pragma once

#include "./util.hpp"

namespace asrtl
{

template < typename CB >
inline status sender_cb( void* data, chann_id id, rec_span* buff )
{
        auto* cb = reinterpret_cast< CB* >( data );
        return ( *cb )( id, buff );
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
