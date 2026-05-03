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

#include "../asrtc/cntr_assm.h"
#include "../asrtc/result.h"
#include "../asrtl/asrt_assert.h"
#include "../asrtlpp/callback.hpp"
#include "../asrtlpp/task.hpp"
#include "../asrtlpp/util.hpp"

namespace asrt
{

/// Initialise the controller assembly — wires controller, diag, param, collect and stream
/// channels. @p alloc is used for all dynamic allocations inside the assembly.
ASRT_NODISCARD inline enum asrt_status init( ref< asrt_cntr_assm > assm, asrt_allocator alloc )
{
        return asrt_cntr_assm_init( assm, alloc );
}

/// Advance all assembly modules by one tick.
/// Must be called periodically from the event loop.
inline void tick( ref< asrt_cntr_assm > assm, uint32_t now )
{
        asrt_cntr_assm_tick( assm, now );
}

/// Execute test @p tid using the full assembly protocol sequence (param upload, collect ready,
/// test exec).  @p tree may be nullptr to skip the param upload step.
/// @p cb is invoked exactly once with the test result or the first error status.
ASRT_NODISCARD inline enum asrt_status exec_test(
    ref< asrt_cntr_assm >             assm,
    asrt_flat_tree const*             tree,
    asrt_flat_id                      root_id,
    uint16_t                          tid,
    uint32_t                          timeout,
    callback< asrt_assembly_exec_cb > cb )
{
        return asrt_cntr_assm_exec_test( assm, tree, root_id, tid, timeout, cb.fn, cb.ptr );
}

/// Release all resources owned by the assembly.
inline void deinit( ref< asrt_cntr_assm > assm )
{
        asrt_cntr_assm_deinit( assm );
}

// ---------------------------------------------------------------------------
// Sender-based variants — for use with co_await in coroutine tasks.

/// Context for cntr_assm_exec_test_sender: runs the full single-test protocol sequence.
struct _cntr_assm_exec_test_ctx
{
        using completion_signatures = ecor::
            completion_signatures< ecor::set_value_t( asrt_result ), ecor::set_error_t( status ) >;

        asrt_cntr_assm*       assm;
        asrt_flat_tree const* tree;
        asrt_flat_id          root_id;
        uint16_t              tid;
        uint32_t              timeout;

        template < typename OP >
        void start( OP& op )
        {
                auto s = asrt_cntr_assm_exec_test(
                    assm,
                    tree,
                    root_id,
                    tid,
                    timeout,
                    +[]( void* p, asrt_status s, asrt_result* res ) -> asrt_status {
                            auto& o = *static_cast< OP* >( p );
                            if ( s != ASRT_SUCCESS )
                                    o.receiver.set_error( s );
                            else
                                    o.receiver.set_value( *res );
                            return ASRT_SUCCESS;
                    },
                    &op );
                if ( s != ASRT_SUCCESS )
                        op.receiver.set_error( s );
        }
};

/// Sender backing co_await exec_test(assm, tree, root_id, tid, timeout).
/// Runs the full single-test protocol sequence; completes with asrt_result.
using cntr_assm_exec_test_sender = ecor::sender_from< _cntr_assm_exec_test_ctx >;

/// co_await exec_test(assm, tree, root_id, tid, timeout) — runs the full single-test sequence;
/// completes with asrt_result.
inline ecor::sender auto exec_test(
    ref< asrt_cntr_assm > assm,
    asrt_flat_tree const* tree,
    asrt_flat_id          root_id,
    uint16_t              tid,
    uint32_t              timeout )
{
        return cntr_assm_exec_test_sender{ { assm, tree, root_id, tid, timeout } };
}

}  // namespace asrt
