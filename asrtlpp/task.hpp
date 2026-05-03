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

#include "../asrtl/status.h"

#include <ecor/ecor.hpp>

namespace asrt
{

struct malloc_free_memory_resource
{
        void* allocate( std::size_t n, std::size_t ) { return malloc( n ); }

        void deallocate( void* p, std::size_t, std::size_t ) noexcept { std::free( p ); }
};

/// Event-loop context that owns a coroutine task scheduler.
/// Call tick() once per main-loop iteration to drive all pending tasks.
struct task_ctx
{
        task_ctx( auto& mem_res )
          : _memory_resource( mem_res )
        {
        }

        void tick() { _core.run_once(); }

        auto& query( ecor::get_memory_resource_t ) { return _memory_resource; }

        auto& query( ecor::get_task_core_t ) { return _core; }

        void reschedule( ecor::schedulable& s ) { _core.reschedule( s ); }

private:
        ecor::task_memory_resource _memory_resource;
        ecor::task_core            _core;
};

using status = asrt_status;

struct test_fail_t
{
};
static constexpr test_fail_t test_fail{};

/// Error signatures and trace policy used by all asrt tasks.
struct task_cfg
{
        using extra_error_signatures = ecor::
            completion_signatures< ecor::set_error_t( status ), ecor::set_error_t( test_fail_t ) >;
        using trace_type = ecor::task_default_trace;
};

/// Coroutine task type used throughout asrt's C++ layer.
/// Supports asrt::status and asrt::test_fail_t error channels.
template < typename T >
using task = ecor::task< T, asrt::task_cfg >;


/// Generic sender adapter.  Given a context type T that provides value_sig
/// and implements start(*this, op), wraps it into a sender that also propagates
/// asrt::status as an error channel.
template < typename T >
struct gen_sender
{
        using sender_concept = ecor::sender_t;
        using context_type   = T;
        using value_sig      = T::value_sig;

        T ctx;

        template < typename... Args >
        gen_sender( Args&&... args )
          : ctx( (Args&&) args... )
        {
        }

        using completion_signatures =
            ecor::completion_signatures< value_sig, ecor::set_error_t( asrt::status ) >;

        template < typename R >
        struct _op
        {
                R            recv;
                context_type ctx;

                void start() { ctx.start( *this ); }
        };

        template < typename R >
        _op< R > connect( R&& receiver ) && noexcept
        {
                return { (R&&) receiver, std::move( ctx ) };
        }
};

/// Sender that completes with success if the contained status is ASRT_SUCCESS, and with test_fail
/// otherwise. Can also be used as simple status wrapper - is comparable with status and convertible
/// to status.
struct status_sender
{
        using sender_concept = ecor::sender_t;
        using completion_signatures =
            ecor::completion_signatures< ecor::set_value_t(), ecor::set_error_t( status ) >;

        asrt_status status;

        status_sender( asrt_status s )
          : status( s )
        {
        }

        template < ecor::receiver R >
        struct op
        {
                asrt_status s;
                R           recv;

                void start()
                {
                        if ( s == ASRT_SUCCESS )
                                recv.set_value();
                        else
                                recv.set_error( s );
                }
        };

        template < ecor::receiver R >
        auto connect( R&& r ) && noexcept
        {
                return op< R >{ status, (R&&) r };
        }

        constexpr      operator asrt_status() const { return status; }
        constexpr bool operator==( asrt_status st ) const { return status == st; }
};

}  // namespace asrt
