#pragma once

#include "../asrtlpp/flat_type_traits.hpp"
#include "../asrtlpp/sender.hpp"
#include "../asrtr/collect.h"

#include <concepts>

namespace asrtr
{

/// Trait mapping C++ types to flat_value scalar member pointers.
/// Specialised for uint32_t, int32_t, float, char const*, bool,
/// and the container tags asrtl::obj / asrtl::arr.
template < typename T >
struct collect_append_traits;

template <>
struct collect_append_traits< uint32_t > : asrtl::flat_type_traits< uint32_t >
{
        static constexpr auto member = &asrtl_flat_scalar::u32_val;
};

template <>
struct collect_append_traits< int32_t > : asrtl::flat_type_traits< int32_t >
{
        static constexpr auto member = &asrtl_flat_scalar::i32_val;
};

template <>
struct collect_append_traits< float > : asrtl::flat_type_traits< float >
{
        static constexpr auto member = &asrtl_flat_scalar::float_val;
};

template <>
struct collect_append_traits< char const* > : asrtl::flat_type_traits< char const* >
{
        static constexpr auto member = &asrtl_flat_scalar::str_val;
};

template <>
struct collect_append_traits< bool > : asrtl::flat_type_traits< bool >
{
        static constexpr auto member = &asrtl_flat_scalar::bool_val;
};

template <>
struct collect_append_traits< asrtl::obj > : asrtl::flat_type_traits< asrtl::obj >
{
};

template <>
struct collect_append_traits< asrtl::arr > : asrtl::flat_type_traits< asrtl::arr >
{
};

/// Concept: T is a scalar type with a flat_value member pointer.
template < typename T >
concept collect_scalar = collect_append_traits< T >::is_scalar;

/// Concept: T is a container tag (OBJECT or ARRAY).
template < typename T >
concept collect_container = !collect_append_traits< T >::is_scalar;

/// C++ wrapper for the reactor-side collect client.
///
/// Provides type-safe append<T>() methods that map C++ types to the
/// underlying flat_value encoding.  Node IDs are auto-assigned.
///
/// Usage:
///   collect_client cc{prev_node, send_cb};
///   // ... after READY handshake ...
///   cc.append<uint32_t>(root, "count", 42);
///   asrtl::flat_id obj;
///   cc.append<asrtl::obj>(root, "sub", obj);
///   cc.append<uint32_t>(obj, "x", 1);
struct collect_client
{
        template < asrtl::sender_callable CB >
        collect_client( asrtl_node* prev, CB& send_cb )
        {
                std::ignore =
                    asrtr_collect_client_init( &client_, prev, asrtl::make_sender( send_cb ) );
        }

        collect_client( asrtl_node* prev, asrtl_sender sender )
        {
                std::ignore = asrtr_collect_client_init( &client_, prev, sender );
        }

        collect_client( collect_client&& )      = delete;
        collect_client( collect_client const& ) = delete;

        asrtl_node* node()
        {
                return &client_.node;
        }

        asrtl_status tick()
        {
                return asrtr_collect_client_tick( &client_ );
        }

        [[nodiscard]] asrtl::flat_id root_id() const
        {
                return asrtr_collect_client_root_id( &client_ );
        }

        /// Scalar append with key: append<uint32_t>(parent, "key", 42).
        template < collect_scalar T >
        asrtl_status append( asrtl::flat_id parent, char const* key, T val )
        {
                using traits             = collect_append_traits< T >;
                using member_type        = decltype( asrtl_flat_scalar{}.*traits::member );
                asrtl_flat_value v       = { .type = traits::flat_type };
                v.data.s.*traits::member = static_cast< member_type >( val );
                return asrtr_collect_client_append( &client_, parent, key, &v, nullptr );
        }

        /// Scalar append without key (array child): append<uint32_t>(parent, 42).
        template < collect_scalar T >
        asrtl_status append( asrtl::flat_id parent, T val )
        {
                return append< T >( parent, nullptr, val );
        }

        /// Container append with key: append<asrtl::obj>(parent, "key", out_id).
        /// @param out Receives the auto-assigned node ID for the new container.
        template < collect_container T >
        asrtl_status append( asrtl::flat_id parent, char const* key, asrtl::flat_id& out )
        {
                using traits       = collect_append_traits< T >;
                asrtl_flat_value v = { .type = traits::flat_type };
                return asrtr_collect_client_append( &client_, parent, key, &v, &out );
        }

        /// Container append without key (array child): append<asrtl::obj>(parent, out_id).
        template < collect_container T >
        asrtl_status append( asrtl::flat_id parent, asrtl::flat_id& out )
        {
                return append< T >( parent, nullptr, out );
        }

        ~collect_client() = default;

private:
        asrtr_collect_client client_;
};

}  // namespace asrtr
