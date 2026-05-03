
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

/// Sender backing co_await start(c, timeout).
/// Performs the protocol handshake; completes with void on success.
struct cntr_start_sender
{
        using sender_concept = ecor::sender_t;
        using completion_signatures =
            ecor::completion_signatures< ecor::set_value_t(), ecor::set_error_t( status ) >;

        asrt_controller* c;
        uint32_t         timeout;

        template < ecor::receiver R >
        struct op
        {
                asrt_controller* c;
                uint32_t         timeout;
                R                recv;

                void start()
                {
                        auto s = asrt_cntr_start(
                            c,
                            +[]( void* p, asrt_status s ) -> asrt_status {
                                    auto& self = *static_cast< op* >( p );
                                    if ( s != ASRT_SUCCESS )
                                            self.recv.set_error( s );
                                    else
                                            self.recv.set_value();
                                    return ASRT_SUCCESS;
                            },
                            this,
                            timeout );
                        if ( s != ASRT_SUCCESS )
                                recv.set_error( s );
                }
        };

        template < ecor::receiver R >
        auto connect( R&& r ) && noexcept
        {
                return op< R >{ c, timeout, (R&&) r };
        }
};

/// Sender backing co_await query_desc(c, timeout).
/// Fetches the target description string; completes with std::string.
struct cntr_query_desc_sender
{
        using sender_concept        = ecor::sender_t;
        using completion_signatures = ecor::
            completion_signatures< ecor::set_value_t( std::string ), ecor::set_error_t( status ) >;

        asrt_controller* c;
        uint32_t         timeout;

        template < ecor::receiver R >
        struct op
        {
                asrt_controller* c;
                uint32_t         timeout;
                R                recv;

                void start()
                {
                        auto s = asrt_cntr_desc(
                            c,
                            +[]( void* p, asrt_status s, char const* desc ) -> asrt_status {
                                    auto& self = *static_cast< op* >( p );
                                    if ( s != ASRT_SUCCESS )
                                            self.recv.set_error( s );
                                    else
                                            self.recv.set_value( std::string{ desc } );
                                    return ASRT_SUCCESS;
                            },
                            this,
                            timeout );
                        if ( s != ASRT_SUCCESS )
                                recv.set_error( s );
                }
        };

        template < ecor::receiver R >
        auto connect( R&& r ) && noexcept
        {
                return op< R >{ c, timeout, (R&&) r };
        }
};

/// Sender backing co_await query_test_count(c, timeout).
/// Fetches the number of registered tests; completes with uint16_t.
struct cntr_query_test_count_sender
{
        using sender_concept        = ecor::sender_t;
        using completion_signatures = ecor::
            completion_signatures< ecor::set_value_t( uint16_t ), ecor::set_error_t( status ) >;

        asrt_controller* c;
        uint32_t         timeout;

        template < ecor::receiver R >
        struct op
        {
                asrt_controller* c;
                uint32_t         timeout;
                R                recv;

                void start()
                {
                        auto s = asrt_cntr_test_count(
                            c,
                            +[]( void* p, asrt_status s, uint16_t count ) -> asrt_status {
                                    auto& self = *static_cast< op* >( p );
                                    if ( s != ASRT_SUCCESS )
                                            self.recv.set_error( s );
                                    else
                                            self.recv.set_value( count );
                                    return ASRT_SUCCESS;
                            },
                            this,
                            timeout );
                        if ( s != ASRT_SUCCESS )
                                recv.set_error( s );
                }
        };

        template < ecor::receiver R >
        auto connect( R&& r ) && noexcept
        {
                return op< R >{ c, timeout, (R&&) r };
        }
};

/// Sender backing co_await query_test_info(c, id, timeout).
/// Fetches name and metadata for test @p id; completes with (uint16_t tid, std::string name).
struct cntr_query_test_info_sender
{
        using sender_concept        = ecor::sender_t;
        using completion_signatures = ecor::completion_signatures<
            ecor::set_value_t( uint16_t, std::string ),
            ecor::set_error_t( status ) >;

        asrt_controller* c;
        uint16_t         id;
        uint32_t         timeout;

        template < ecor::receiver R >
        struct op
        {
                asrt_controller* c;
                uint16_t         id;
                uint32_t         timeout;
                R                recv;

                void start()
                {
                        auto s = asrt_cntr_test_info(
                            c,
                            id,
                            +[]( void* p, asrt_status s, uint16_t tid, char const* desc )
                                -> asrt_status {
                                    auto& self = *static_cast< op* >( p );
                                    if ( s != ASRT_SUCCESS )
                                            self.recv.set_error( s );
                                    else
                                            self.recv.set_value( tid, std::string{ desc } );
                                    return ASRT_SUCCESS;
                            },
                            this,
                            timeout );
                        if ( s != ASRT_SUCCESS )
                                recv.set_error( s );
                }
        };

        template < ecor::receiver R >
        auto connect( R&& r ) && noexcept
        {
                return op< R >{ c, id, timeout, (R&&) r };
        }
};

/// Sender backing co_await exec_test(c, id, timeout).
/// Executes test @p id on the target; completes with asrt_result.
struct cntr_exec_test_sender
{
        using sender_concept        = ecor::sender_t;
        using completion_signatures = ecor::
            completion_signatures< ecor::set_value_t( asrt_result ), ecor::set_error_t( status ) >;

        asrt_controller* c;
        uint16_t         id;
        uint32_t         timeout;

        template < ecor::receiver R >
        struct op
        {
                asrt_controller* c;
                uint16_t         id;
                uint32_t         timeout;
                R                recv;

                void start()
                {
                        auto s = asrt_cntr_test_exec(
                            c,
                            id,
                            +[]( void* p, asrt_status s, asrt_result* res ) -> asrt_status {
                                    auto& self = *static_cast< op* >( p );
                                    if ( s != ASRT_SUCCESS )
                                            self.recv.set_error( s );
                                    else
                                            self.recv.set_value( *res );
                                    return ASRT_SUCCESS;
                            },
                            this,
                            timeout );
                        if ( s != ASRT_SUCCESS )
                                recv.set_error( s );
                }
        };

        template < ecor::receiver R >
        auto connect( R&& r ) && noexcept
        {
                return op< R >{ c, id, timeout, (R&&) r };
        }
};

/// co_await start(c, timeout) — performs the protocol handshake; completes with void on success.
inline ecor::sender auto start( ref< asrt_controller > c, uint32_t timeout )
{
        return cntr_start_sender{ c, timeout };
}

/// co_await query_desc(c, timeout) — fetches the target description; completes with std::string.
inline ecor::sender auto query_desc( ref< asrt_controller > c, uint32_t timeout )
{
        return cntr_query_desc_sender{ c, timeout };
}

/// co_await query_test_count(c, timeout) — fetches the number of registered tests; completes with
/// uint16_t.
inline ecor::sender auto query_test_count( ref< asrt_controller > c, uint32_t timeout )
{
        return cntr_query_test_count_sender{ c, timeout };
}

/// co_await query_test_info(c, id, timeout) — fetches name for test id; completes with (uint16_t
/// tid, std::string name).
inline ecor::sender auto query_test_info( ref< asrt_controller > c, uint16_t id, uint32_t timeout )
{
        return cntr_query_test_info_sender{ c, id, timeout };
}

/// co_await exec_test(c, id, timeout) — executes test id; completes with asrt_result.
inline ecor::sender auto exec_test( ref< asrt_controller > c, uint16_t id, uint32_t timeout )
{
        return cntr_exec_test_sender{ c, id, timeout };
}

}  // namespace asrt
