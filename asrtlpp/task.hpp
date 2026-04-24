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

struct task_ctx
{
        task_ctx( auto& mem_res )
          : memory_resource( mem_res )
        {
        }

        void tick() { core.run_once(); }

        auto& query( ecor::get_memory_resource_t ) { return memory_resource; }

        auto& query( ecor::get_task_core_t ) { return core; }

        void reschedule( ecor::schedulable& s ) { core.reschedule( s ); }

private:
        ecor::task_memory_resource memory_resource;
        ecor::task_core            core;
};

using status = asrtl_status;

struct test_fail_t
{
};
static constexpr test_fail_t test_fail{};

struct task_cfg
{
        using extra_error_signatures = ecor::
            completion_signatures< ecor::set_error_t( status ), ecor::set_error_t( test_fail_t ) >;
        using trace_type = ecor::task_default_trace;
};

template < typename T >
using task = ecor::task< T, asrt::task_cfg >;


template < typename T >
struct gen_sender
{
        using sender_concept = ecor::sender_t;
        using context_type   = T;
        using value_sig      = typename T::value_sig;

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
                return { std::move( receiver ), std::move( ctx ) };
        }
};


}  // namespace asrt
