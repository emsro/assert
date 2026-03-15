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

#include <cstdint>
#include <cstdlib>

/// Tracks allocations/frees and can fail on the N-th allocation call.
/// Set fail_at_call to 1 to fail immediately on the first call, 2 for the
/// second, etc.  Set to 0 (default) to never fail.
struct stub_allocator_ctx
{
        uint32_t alloc_calls  = 0;
        uint32_t free_calls   = 0;
        uint32_t fail_at_call = 0;  ///< 1-based; 0 = never fail
};

static inline void* stub_alloc_fn( void* ptr, uint32_t size )
{
        auto* ctx = static_cast< stub_allocator_ctx* >( ptr );
        ctx->alloc_calls++;
        if ( ctx->fail_at_call != 0 && ctx->alloc_calls >= ctx->fail_at_call )
                return nullptr;
        return std::malloc( size );
}

static inline void stub_free_fn( void* ptr, void* mem )
{
        auto* ctx = static_cast< stub_allocator_ctx* >( ptr );
        ctx->free_calls++;
        std::free( mem );
}

static inline asrtl_allocator asrtl_stub_allocator( stub_allocator_ctx* ctx )
{
        return asrtl_allocator{
            .ptr   = ctx,
            .alloc = &stub_alloc_fn,
            .free  = &stub_free_fn,
        };
}
