#pragma once

#include "../asrtl/chann.h"

#include <concepts>
#include <span>

namespace asrtl
{
using chann_id = asrtl_chann_id;
using status   = asrtl_status;
}  // namespace asrtl

namespace asrtr
{

template < typename CB >
concept send_cb = std::invocable< CB, asrtl::chann_id, std::span< std::byte > >;

inline asrtl_span cnv( std::span< std::byte > buff )
{
        return {
            .b = (uint8_t*) buff.data(),
            .e = (uint8_t*) buff.data() + buff.size(),
        };
}

inline std::span< std::byte > cnv( asrtl_span buff )
{
        return { (std::byte*) buff.b, (std::byte*) buff.e };
}

template < send_cb CB >
inline asrtl::status _send_cb( void* data, asrtl::chann_id id, asrtl_span buff )
{
        auto*                  cb = reinterpret_cast< CB* >( data );
        std::span< std::byte > sp = cnv( buff );
        return ( *cb )( id, sp );
}

template < send_cb CB >
inline asrtl_sender make_sender( CB& cb )
{
        return {
            .ptr = &cb,
            .cb  = &_send_cb< CB >,
        };
}

}  // namespace asrtr
