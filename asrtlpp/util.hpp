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

#include "../asrtl/allocator.h"
#include "../asrtl/asrt_assert.h"
#include "../asrtl/chann.h"
#include "../asrtl/source.h"

#include <functional>
#include <memory>
#include <optional>
#include <span>

namespace asrt
{
using source    = asrt_source;
using chann_id  = asrt_chann_id;
using status    = asrt_status;
using allocator = asrt_allocator;
using span      = asrt_span;
using rec_span  = asrt_rec_span;
using send_cb   = std::function< asrt_status( chann_id, rec_span& ) >;
template < typename T >
using uptr = std::unique_ptr< T >;
template < typename T >
using opt = std::optional< T >;

inline asrt_span cnv( std::span< uint8_t > buff )
{
        return {
            .b = buff.data(),
            .e = buff.data() + buff.size(),
        };
}

inline std::span< uint8_t > cnv( asrt_span buff )
{
        return { buff.b, buff.e };
}

inline enum asrt_status dispatch( struct asrt_node& head, std::span< uint8_t > buff )
{
        return asrt_chann_dispatch(
            &head, asrt_span{ .b = buff.data(), .e = buff.data() + buff.size() } );
}

inline enum asrt_status tick( struct asrt_node& head, uint32_t now )
{
        return asrt_chann_tick( &head, now );
}

inline asrt_node& node( auto* comp )
{
        return comp->node;
}

inline asrt_node& node( auto& comp )
{
        return comp.node;
}

inline status recv( asrt_node& n, std::span< uint8_t > buff )
{
        return asrt_chann_recv( &n, cnv( buff ) );
}

inline status recv( asrt_node& n, asrt_span buff )
{
        return asrt_chann_recv( &n, buff );
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
                ASRT_ASSERT( t != nullptr );
        }

private:
        T* p;
};

}  // namespace asrt
