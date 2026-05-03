
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

#include "../asrtc/controller.h"
#include "../asrtc/result.h"
#include "../asrtlpp/callback.hpp"
#include "../asrtlpp/task.hpp"
#include "../asrtlpp/util.hpp"

#include <functional>
#include <string>

namespace asrt
{
using status = asrt_status;
using result = asrt_result;

/// Initialise a controller, wiring it to @p s for outgoing messages.
ASRT_NODISCARD inline status init( ref< asrt_controller > c, asrt_send_req_list* s, allocator a )
{
        return asrt_cntr_init( c, s, a );
}

/// Begin the protocol handshake; @p cb is invoked once the target responds or the operation times
/// out.
ASRT_NODISCARD inline status start(
    ref< asrt_controller >         c,
    callback< asrt_init_callback > cb,
    uint32_t                       timeout )
{
        return asrt_cntr_start( c, cb.fn, cb.ptr, timeout );
}

/// Returns true if the controller has no pending operation.
ASRT_NODISCARD inline bool is_idle( ref< asrt_controller > c )
{
        return asrt_cntr_idle( c ) > 0;
}

/// Request the target description string; @p cb receives it on success.
ASRT_NODISCARD inline status query_desc(
    ref< asrt_controller >         c,
    callback< asrt_desc_callback > cb,
    uint32_t                       timeout )
{
        return asrt_cntr_desc( c, cb.fn, cb.ptr, timeout );
}

/// Request the number of tests registered on the target; @p cb receives the count.
ASRT_NODISCARD inline status query_test_count(
    ref< asrt_controller >               c,
    callback< asrt_test_count_callback > cb,
    uint32_t                             timeout )
{
        return asrt_cntr_test_count( c, cb.fn, cb.ptr, timeout );
}

/// Request name and metadata for test @p id; @p cb receives the info.
ASRT_NODISCARD inline status query_test_info(
    ref< asrt_controller >              c,
    uint16_t                            id,
    callback< asrt_test_info_callback > cb,
    uint32_t                            timeout )
{
        return asrt_cntr_test_info( c, id, cb.fn, cb.ptr, timeout );
}

/// Execute test @p id on the target; @p cb receives the pass/fail result.
ASRT_NODISCARD inline status exec_test(
    ref< asrt_controller >                c,
    uint16_t                              id,
    callback< asrt_test_result_callback > cb,
    uint32_t                              timeout )
{
        return asrt_cntr_test_exec( c, id, cb.fn, cb.ptr, timeout );
}

/// Release all resources owned by the controller.
inline void deinit( ref< asrt_controller > c )
{
        asrt_cntr_deinit( c );
}

// ---------------------------------------------------------------------------
// Sender-based variants — for use with co_await in coroutine tasks.

/// Context for cntr_start_sender: performs the protocol handshake.
struct _cntr_start_ctx
{
        using completion_signatures =
            ecor::completion_signatures< ecor::set_value_t(), ecor::set_error_t( status ) >;

        asrt_controller* c;
        uint32_t         timeout;

        template < typename OP >
        void start( OP& op )
        {
                auto s = asrt_cntr_start(
                    c,
                    +[]( void* p, asrt_status s ) -> asrt_status {
                            auto& o = *static_cast< OP* >( p );
                            if ( s != ASRT_SUCCESS )
                                    o.receiver.set_error( s );
                            else
                                    o.receiver.set_value();
                            return ASRT_SUCCESS;
                    },
                    &op,
                    timeout );
                if ( s != ASRT_SUCCESS )
                        op.receiver.set_error( s );
        }
};

/// Sender backing co_await start(c, timeout).
/// Performs the protocol handshake; completes with void on success.
using cntr_start_sender = ecor::sender_from< _cntr_start_ctx >;

/// Context for cntr_query_desc_sender: fetches the target description string.
struct _cntr_query_desc_ctx
{
        using completion_signatures = ecor::
            completion_signatures< ecor::set_value_t( std::string ), ecor::set_error_t( status ) >;

        asrt_controller* c;
        uint32_t         timeout;

        template < typename OP >
        void start( OP& op )
        {
                auto s = asrt_cntr_desc(
                    c,
                    +[]( void* p, asrt_status s, char const* desc ) -> asrt_status {
                            auto& o = *static_cast< OP* >( p );
                            if ( s != ASRT_SUCCESS )
                                    o.receiver.set_error( s );
                            else
                                    o.receiver.set_value( std::string{ desc } );
                            return ASRT_SUCCESS;
                    },
                    &op,
                    timeout );
                if ( s != ASRT_SUCCESS )
                        op.receiver.set_error( s );
        }
};

/// Sender backing co_await query_desc(c, timeout).
/// Fetches the target description string; completes with std::string.
using cntr_query_desc_sender = ecor::sender_from< _cntr_query_desc_ctx >;

/// Context for cntr_query_test_count_sender: fetches the number of registered tests.
struct _cntr_query_test_count_ctx
{
        using completion_signatures = ecor::
            completion_signatures< ecor::set_value_t( uint16_t ), ecor::set_error_t( status ) >;

        asrt_controller* c;
        uint32_t         timeout;

        template < typename OP >
        void start( OP& op )
        {
                auto s = asrt_cntr_test_count(
                    c,
                    +[]( void* p, asrt_status s, uint16_t count ) -> asrt_status {
                            auto& o = *static_cast< OP* >( p );
                            if ( s != ASRT_SUCCESS )
                                    o.receiver.set_error( s );
                            else
                                    o.receiver.set_value( count );
                            return ASRT_SUCCESS;
                    },
                    &op,
                    timeout );
                if ( s != ASRT_SUCCESS )
                        op.receiver.set_error( s );
        }
};

/// Sender backing co_await query_test_count(c, timeout).
/// Fetches the number of registered tests; completes with uint16_t.
using cntr_query_test_count_sender = ecor::sender_from< _cntr_query_test_count_ctx >;

/// Context for cntr_query_test_info_sender: fetches name and metadata for a test.
struct _cntr_query_test_info_ctx
{
        using completion_signatures = ecor::completion_signatures<
            ecor::set_value_t( uint16_t, std::string ),
            ecor::set_error_t( status ) >;

        asrt_controller* c;
        uint16_t         id;
        uint32_t         timeout;

        template < typename OP >
        void start( OP& op )
        {
                auto s = asrt_cntr_test_info(
                    c,
                    id,
                    +[]( void* p, asrt_status s, uint16_t tid, char const* desc ) -> asrt_status {
                            auto& o = *static_cast< OP* >( p );
                            if ( s != ASRT_SUCCESS )
                                    o.receiver.set_error( s );
                            else
                                    o.receiver.set_value( tid, std::string{ desc } );
                            return ASRT_SUCCESS;
                    },
                    &op,
                    timeout );
                if ( s != ASRT_SUCCESS )
                        op.receiver.set_error( s );
        }
};

/// Sender backing co_await query_test_info(c, id, timeout).
/// Fetches name and metadata for test @p id; completes with (uint16_t tid, std::string name).
using cntr_query_test_info_sender = ecor::sender_from< _cntr_query_test_info_ctx >;

/// Context for cntr_exec_test_sender: executes a test on the target.
struct _cntr_exec_test_ctx
{
        using completion_signatures = ecor::
            completion_signatures< ecor::set_value_t( asrt_result ), ecor::set_error_t( status ) >;

        asrt_controller* c;
        uint16_t         id;
        uint32_t         timeout;

        template < typename OP >
        void start( OP& op )
        {
                auto s = asrt_cntr_test_exec(
                    c,
                    id,
                    +[]( void* p, asrt_status s, asrt_result* res ) -> asrt_status {
                            auto& o = *static_cast< OP* >( p );
                            if ( s != ASRT_SUCCESS )
                                    o.receiver.set_error( s );
                            else
                                    o.receiver.set_value( *res );
                            return ASRT_SUCCESS;
                    },
                    &op,
                    timeout );
                if ( s != ASRT_SUCCESS )
                        op.receiver.set_error( s );
        }
};

/// Sender backing co_await exec_test(c, id, timeout).
/// Executes test @p id on the target; completes with asrt_result.
using cntr_exec_test_sender = ecor::sender_from< _cntr_exec_test_ctx >;

/// co_await start(c, timeout) — performs the protocol handshake; completes with void on success.
inline ecor::sender auto start( ref< asrt_controller > c, uint32_t timeout )
{
        return cntr_start_sender{ { c, timeout } };
}

/// co_await query_desc(c, timeout) — fetches the target description; completes with std::string.
inline ecor::sender auto query_desc( ref< asrt_controller > c, uint32_t timeout )
{
        return cntr_query_desc_sender{ { c, timeout } };
}

/// co_await query_test_count(c, timeout) — fetches the number of registered tests; completes with
/// uint16_t.
inline ecor::sender auto query_test_count( ref< asrt_controller > c, uint32_t timeout )
{
        return cntr_query_test_count_sender{ { c, timeout } };
}

/// co_await query_test_info(c, id, timeout) — fetches name for test id; completes with (uint16_t
/// tid, std::string name).
inline ecor::sender auto query_test_info( ref< asrt_controller > c, uint16_t id, uint32_t timeout )
{
        return cntr_query_test_info_sender{ { c, id, timeout } };
}

/// co_await exec_test(c, id, timeout) — executes test id; completes with asrt_result.
inline ecor::sender auto exec_test( ref< asrt_controller > c, uint16_t id, uint32_t timeout )
{
        return cntr_exec_test_sender{ { c, id, timeout } };
}

}  // namespace asrt
