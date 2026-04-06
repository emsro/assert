#pragma once

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

template < typename T >
using task = asrtl::task< T, status >;

}  // namespace asrtio
