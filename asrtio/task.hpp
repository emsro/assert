#pragma once

#include "../asrtl/log.h"
#include "../asrtlpp/task.hpp"

#include <ecor/ecor.hpp>

namespace asrtio
{

enum class status
{
        success,
        connect_failed,
        query_failed,
        init_failed,
        bind_failed,
        listen_failed,
        send_failed,
};

inline char const* status_to_str( status s )
{
        switch ( s ) {
        case status::success:
                return "success";
        case status::connect_failed:
                return "connect_failed";
        case status::query_failed:
                return "query_failed";
        case status::init_failed:
                return "init_failed";
        case status::bind_failed:
                return "bind_failed";
        case status::listen_failed:
                return "listen_failed";
        case status::send_failed:
                return "send_failed";
        }
        return "unknown_status";
}

using asrtl::task_ctx;
using mem_res = asrtl::malloc_free_memory_resource;
using arena   = ecor::async_arena< task_ctx, mem_res >;

template < typename T >
using async_ptr = ecor::async_ptr< T, task_ctx, mem_res >;

template < typename T >
using task = asrtl::task< T, status >;

// XXX: specific version of just sender? maybe move to ecor?
template < typename S >
struct _complete_arena_sender
{

        using sender_concept = ecor::sender_t;

        arena& a;
        S      s;

        template < typename R >
        struct op : ecor::schedulable
        {
                using operation_state_concept = ecor::operation_state_t;
                using arena_sender            = decltype( std::declval< arena >().async_destroy() );

                struct recv;

                using op1_t = ecor::connect_type< S, recv >;
                using op2_t = ecor::connect_type< arena_sender, R >;

                struct recv
                {
                        using receiver_concept = ecor::receiver_t;

                        op& o;

                        recv( op& o )
                          : o( o )
                        {
                        }

                        recv( recv&& ) noexcept = default;

                        void set_value()
                        {
                                ASRTL_INF_LOG(
                                    "asrtio_task", "Task completed, starting arena cleanup" );
                                o.advance();
                        }

                        void set_error( auto ) noexcept
                        {
                                ASRTL_INF_LOG(
                                    "asrtio_task",
                                    "Task completed with error, starting arena cleanup" );
                                o.advance();
                        }

                        void set_stopped() noexcept
                        {
                                ASRTL_INF_LOG(
                                    "asrtio_task", "Task stopped, starting arena cleanup" );
                                o.advance();
                        }

                        auto get_env() const noexcept
                        {
                                return ecor::empty_env{};
                        }

                        ~recv() noexcept = default;
                };

                arena&                                       a;
                S                                            s;
                R                                            r;
                std::variant< std::monostate, op1_t, op2_t > ops;

                op( arena& a, S s, R r )
                  : a( a )
                  , s( std::move( s ) )
                  , r( std::move( r ) )
                {
                }

                void start()
                {
                        auto& op = ops.template emplace< op1_t >(
                            std::move( s ).connect( recv{ *this } ) );
                        op.start();
                }

                void advance()
                {
                        a.ctx().reschedule( *this );
                }

                void resume() override
                {
                        auto& op = ops.template emplace< op2_t >(
                            a.async_destroy().connect( std::move( r ) ) );
                        op.start();
                }
        };

        template < typename R >
        auto connect( R&& r ) &&
        {
                return op< R >( a, std::move( s ), (R&&) r );
        }
};

struct _complete_closure
{
        arena& a;
};

template < typename S >
auto operator|( S&& s, _complete_closure&& c )
{
        return _complete_arena_sender< std::decay_t< S > >{ c.a, std::forward< S >( s ) };
}

inline auto complete_arena( arena& a )
{
        return _complete_closure{ a };
}

template < typename S, typename R >
using complete_arena_connect_result =
    decltype( ( std::declval< S >() | complete_arena( std::declval< arena& >() ) )
                  .connect( std::declval< R >() ) );


}  // namespace asrtio
