#pragma once

#include "./collect.hpp"
#include "./task_unit.hpp"

namespace asrtr
{

/// Sender that appends a single node to the collect client's tree.
///
/// Two partial specialisations:
/// - collect_scalar T: completes with set_value() on success.
/// - collect_container T: completes with set_value(flat_id) returning the new node ID.
///
/// These are instantiated via the free-function overloads below.
template < typename T >
struct collect_append_sender;

// Scalar sender: co_await append<uint32_t>(cc, parent, "key", 42)
template < collect_scalar T >
struct collect_append_sender< T >
{
        using sender_concept = ecor::sender_t;
        using completion_signatures =
            ecor::completion_signatures< ecor::set_value_t(), ecor::set_error_t( task_error ) >;

        collect_client*                                 client_;
        asrtl::flat_id                                  parent;
        char const*                                     key;
        typename collect_append_traits< T >::value_type val;

        template < ecor::receiver R >
        struct op
        {
                collect_append_sender s;
                R                     recv;

                void start()
                {
                        auto st = s.client_->template append< T >( s.parent, s.key, s.val );
                        if ( st == ASRTL_SUCCESS )
                                recv.set_value();
                        else
                                recv.set_error( task_error::test_fail );
                }
        };

        template < ecor::receiver R >
        auto connect( R&& r ) && noexcept
        {
                return op< R >{ std::move( *this ), (R&&) r };
        }
};

// Container sender: auto id = co_await append<asrtl::obj>(cc, parent, "key")
template < collect_container T >
struct collect_append_sender< T >
{
        using sender_concept        = ecor::sender_t;
        using completion_signatures = ecor::completion_signatures<
            ecor::set_value_t( asrtl::flat_id ),
            ecor::set_error_t( task_error ) >;

        collect_client* client_;
        asrtl::flat_id  parent;
        char const*     key;

        template < ecor::receiver R >
        struct op
        {
                collect_append_sender s;
                R                     recv;

                void start()
                {
                        asrtl::flat_id out = 0;
                        auto           st = s.client_->template append< T >( s.parent, s.key, out );
                        if ( st == ASRTL_SUCCESS )
                                recv.set_value( out );
                        else
                                recv.set_error( task_error::test_fail );
                }
        };

        template < ecor::receiver R >
        auto connect( R&& r ) && noexcept
        {
                return op< R >{ std::move( *this ), (R&&) r };
        }
};

/// co_await append<T>(client, parent, key, val) — scalar with key.
template < collect_scalar T >
ecor::sender auto append( collect_client& client, asrtl::flat_id parent, char const* key, T val )
{
        return collect_append_sender< T >{ &client, parent, key, val };
}

/// co_await append<T>(client, parent, val) — scalar without key (array child).
template < collect_scalar T >
ecor::sender auto append( collect_client& client, asrtl::flat_id parent, T val )
{
        return collect_append_sender< T >{ &client, parent, nullptr, val };
}

/// co_await append<T>(client, parent, key) — container with key; returns flat_id.
template < collect_container T >
ecor::sender auto append( collect_client& client, asrtl::flat_id parent, char const* key )
{
        return collect_append_sender< T >{ &client, parent, key };
}

/// co_await append<T>(client, parent) — container without key (array child); returns flat_id.
template < collect_container T >
ecor::sender auto append( collect_client& client, asrtl::flat_id parent )
{
        return collect_append_sender< T >{ &client, parent, nullptr };
}

}  // namespace asrtr
