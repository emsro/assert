#pragma once

#include "../asrtl/allocator.h"
#include "../asrtl/asrtl_assert.h"
#include "../asrtl/chann.h"
#include "../asrtl/ecode.h"
#include "../asrtl/source.h"

#include <functional>
#include <memory>
#include <optional>
#include <span>

namespace asrt
{
using source    = asrtl_source;
using chann_id  = asrtl_chann_id;
using status    = asrtl_status;
using allocator = asrtl_allocator;
using span      = asrtl_span;
using rec_span  = asrtl_rec_span;
using send_cb   = std::function< asrtl_status( chann_id, rec_span& ) >;
using ecode     = asrtl_ecode;
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

inline enum asrtl_status dispatch( struct asrtl_node& head, std::span< uint8_t > buff )
{
        return asrtl_chann_dispatch(
            &head, asrtl_span{ .b = buff.data(), .e = buff.data() + buff.size() } );
}

inline enum asrtl_status tick( struct asrtl_node& head, uint32_t now )
{
        return asrtl_chann_tick( &head, now );
}

inline asrtl_node& node( auto* comp )
{
        return comp->node;
}

inline asrtl_node& node( auto& comp )
{
        return comp.node;
}

inline status recv( asrtl_node& n, std::span< uint8_t > buff )
{
        return asrtl_chann_recv( &n, cnv( buff ) );
}

inline status recv( asrtl_node& n, asrtl_span buff )
{
        return asrtl_chann_recv( &n, buff );
}

template < typename T >
struct ref
{
        T& operator*() { return *p; }
        T* operator->() { return p; }

        operator T&() { return *p; }
        operator T*() { return p; }

        ref( T& t )
          : p( &t )
        {
        }
        ref( T* t )
          : p( t )
        {
                ASRTL_ASSERT( t != nullptr );
        }

private:
        T* p;
};

}  // namespace asrt
