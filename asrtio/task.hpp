#pragma once

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
        }
        return "unknown_status";
}

struct malloc_free_memory_resource
{
        void* allocate( std::size_t n, std::size_t )
        {
                return malloc( n );
        }

        void deallocate( void* p, std::size_t, std::size_t ) noexcept
        {
                std::free( p );
        }
};

struct task_ctx
{

        void tick()
        {
                core.run_once();
        }

        auto& query( ecor::get_memory_resource_t )
        {
                return memory_resource;
        }

        auto& query( ecor::get_task_core_t )
        {
                return core;
        }

private:
        malloc_free_memory_resource res;
        ecor::task_memory_resource  memory_resource{ res };
        ecor::task_core             core;
};

struct task_cfg
{
        using extra_error_signatures = ecor::completion_signatures< ecor::set_error_t( status ) >;
};

template < typename T >
using task = ecor::task< T, task_cfg >;

}  // namespace asrtio
