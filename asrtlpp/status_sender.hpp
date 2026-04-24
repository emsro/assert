#pragma once

#include "./task.hpp"

#include <ecor/ecor.hpp>

namespace asrt
{

/// XXX: Thinks to worry about: we gained plenty of complexity because the ecor:: world has two
/// tasks: one that acceps task_error as an error and one that accepts status.
///
/// -> maybe we can merge these two together?

/// Sender that completes with success if the contained status is ASRTL_SUCCESS, and with test_fail
/// otherwise. Can also be used as simple status wrapper - is comparable with status and convertible
/// to status.
struct status_sender
{
        using sender_concept = ecor::sender_t;
        using completion_signatures =
            ecor::completion_signatures< ecor::set_value_t(), ecor::set_error_t( status ) >;

        asrtl_status status;

        status_sender( asrtl_status s )
          : status( s )
        {
        }

        template < ecor::receiver R >
        struct op
        {
                asrtl_status s;
                R            recv;

                void start()
                {
                        if ( s == ASRTL_SUCCESS )
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

        constexpr      operator asrtl_status() const { return status; }
        constexpr bool operator==( asrtl_status st ) const { return status == st; }
};

}  // namespace asrt
