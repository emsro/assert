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
using send_cb  = std::function< asrtl_status( chann_id, std::span< uint8_t > ) >;
using ecode    = uint16_t;
template < typename T >
using uptr = std::unique_ptr< T >;
template < typename T >
using opt = std::optional< T >;

inline asrtl_span cnv( std::span< uint8_t > buff )
{
        return {
            .b = buff.data(),
            .e = buff.data() + buff.size(),
        };
}

inline std::span< uint8_t > cnv( asrtl_span buff )
{
        return { buff.b, buff.e };
}

inline enum asrtl_status chann_dispatch( struct asrtl_node* head, std::span< uint8_t > buff )
{
        return asrtl_chann_dispatch(
            head, asrtl_span{ .b = buff.data(), .e = buff.data() + buff.size() } );
}

}  // namespace asrtl
