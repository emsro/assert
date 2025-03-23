#pragma once

#include "../asrtl/chann.h"
#include "../asrtl/source.h"

#include <functional>
#include <memory>
#include <span>

namespace asrtl
{
using source   = asrtl_source;
using chann_id = asrtl_chann_id;
using status   = asrtl_status;
using span     = asrtl_span;
using send_cb  = std::function< asrtl_status( chann_id, std::span< std::byte > ) >;
using ecode    = uint16_t;
template < typename T >
using uptr = std::unique_ptr< T >;
template < typename T >
using opt = std::optional< T >;

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

}  // namespace asrtl
