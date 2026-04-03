#pragma once

#include "../asrtlpp/sender.hpp"
#include "../asrtr/param.h"

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace asrtr
{

// Special tag types for object/array
struct param_obj
{
};
struct param_arr
{
};

// Trait system to pick C-API callback based on requested type
template < typename T >
struct param_query_traits;

template <>
struct param_query_traits< uint32_t >
{
        using raw_type                      = uint32_t;
        using value_type                    = uint32_t;
        using cb_type                       = asrtr_param_u32_cb;
        static constexpr auto expected_type = ASRTL_FLAT_VALUE_TYPE_U32;
        static constexpr auto cb_member     = &asrtr_param_cb::u32;
};

template <>
struct param_query_traits< int32_t >
{
        using raw_type                      = int32_t;
        using value_type                    = int32_t;
        using cb_type                       = asrtr_param_i32_cb;
        static constexpr auto expected_type = ASRTL_FLAT_VALUE_TYPE_I32;
        static constexpr auto cb_member     = &asrtr_param_cb::i32;
};

template <>
struct param_query_traits< float >
{
        using raw_type                      = float;
        using value_type                    = float;
        using cb_type                       = asrtr_param_float_cb;
        static constexpr auto expected_type = ASRTL_FLAT_VALUE_TYPE_FLOAT;
        static constexpr auto cb_member     = &asrtr_param_cb::flt;
};

template <>
struct param_query_traits< char const* >
{
        using raw_type                      = char const*;
        using value_type                    = char const*;
        using cb_type                       = asrtr_param_str_cb;
        static constexpr auto expected_type = ASRTL_FLAT_VALUE_TYPE_STR;
        static constexpr auto cb_member     = &asrtr_param_cb::str;
};

template <>
struct param_query_traits< bool >
{
        using raw_type                      = uint32_t;
        using value_type                    = bool;
        using cb_type                       = asrtr_param_bool_cb;
        static constexpr auto expected_type = ASRTL_FLAT_VALUE_TYPE_BOOL;
        static constexpr auto cb_member     = &asrtr_param_cb::bln;
};

template <>
struct param_query_traits< param_obj >
{
        using raw_type                      = asrtl_flat_child_list;
        using value_type                    = asrtl_flat_child_list;
        using cb_type                       = asrtr_param_obj_cb;
        static constexpr auto expected_type = ASRTL_FLAT_VALUE_TYPE_OBJECT;
        static constexpr auto cb_member     = &asrtr_param_cb::obj;
};

template <>
struct param_query_traits< param_arr >
{
        using raw_type                      = asrtl_flat_child_list;
        using value_type                    = asrtl_flat_child_list;
        using cb_type                       = asrtr_param_arr_cb;
        static constexpr auto expected_type = ASRTL_FLAT_VALUE_TYPE_ARRAY;
        static constexpr auto cb_member     = &asrtr_param_cb::arr;
};

template <>
struct param_query_traits< asrtl_flat_value >
{
        using raw_type                      = asrtl_flat_value;
        using value_type                    = asrtl_flat_value;
        using cb_type                       = asrtr_param_any_cb;
        static constexpr auto expected_type = ASRTL_FLAT_VALUE_TYPE_NONE;
        static constexpr auto cb_member     = &asrtr_param_cb::any;
};

// Convenience aliases
template <>
struct param_query_traits< void > : param_query_traits< asrtl_flat_value >
{
};

template < typename T >
concept has_param_query_traits = requires { typename param_query_traits< T >; };

template < typename CB, typename T >
concept typed_param_query_callable =
    has_param_query_traits< T > && requires(
                                       CB                                           cb,
                                       asrtr_param_client*                          c,
                                       asrtr_param_query*                           q,
                                       typename param_query_traits< T >::value_type v ) {
            { cb( c, q, v ) } -> std::same_as< void >;
    };

struct param_client
{
        template < asrtl::sender_callable CB >
        param_client( asrtl_node* prev, CB& send_cb, asrtl_span msg_buffer, uint32_t timeout )
        {
                std::ignore = asrtr_param_client_init(
                    &client_, prev, asrtl::make_sender( send_cb ), msg_buffer, timeout );
        }

        param_client(
            asrtl_node*  prev,
            asrtl_sender sender,
            asrtl_span   msg_buffer,
            uint32_t     timeout )
        {
                std::ignore =
                    asrtr_param_client_init( &client_, prev, sender, msg_buffer, timeout );
        }

        param_client( param_client&& )      = delete;
        param_client( param_client const& ) = delete;

        asrtl_node* node()
        {
                return &client_.node;
        }

        [[nodiscard]] bool ready() const
        {
                return client_.ready != 0;
        }

        [[nodiscard]] bool query_pending() const
        {
                return asrtr_param_query_pending( &client_ ) != 0;
        }


        [[nodiscard]] asrtl_flat_id root_id() const
        {
                return asrtr_param_client_root_id( &client_ );
        }

        /// Fetch information about node node_id from param client, call cb with the value if
        /// successful. cb will be called even on error, with an appropriate error code in
        /// q->error_code and possibly an invalid value. T is the expected type of the value, if
        /// value type doesn't match, cb will be called with an error.
        template < has_param_query_traits T, typed_param_query_callable< T > CB >
        [[nodiscard]] asrtl_status fetch( asrtr_param_query* q, asrtl_flat_id node_id, CB& cb )
        {
                return query< T >( q, node_id, nullptr, cb );
        }

        /// Fetch information about node node_id from param client, call cb with the value if
        /// successful. This overload expects raw function pointer and void* context.
        template < has_param_query_traits T >
        [[nodiscard]] asrtl_status fetch(
            asrtr_param_query*                        q,
            asrtl_flat_id                             node_id,
            typename param_query_traits< T >::cb_type cb,
            void*                                     cb_ptr )
        {
                return query< T >( q, node_id, nullptr, cb, cb_ptr );
        }

        /// Raw fetch without explicit expected type and C callback + void* context.
        [[nodiscard]] asrtl_status fetch(
            asrtr_param_query* q,
            asrtl_flat_id      node_id,
            asrtr_param_any_cb cb,
            void*              cb_ptr )
        {
                return query( q, node_id, nullptr, cb, cb_ptr );
        }

        /// Find child with key under parent_id, call cb with the value if successful.
        template < has_param_query_traits T, typed_param_query_callable< T > CB >
        [[nodiscard]] asrtl_status find(
            asrtr_param_query* q,
            asrtl_flat_id      parent_id,
            char const*        key,
            CB&                cb )
        {
                return query< T >( q, parent_id, key, cb );
        }

        /// Find child with key under parent_id. Raw function pointer + void* context overload.
        template < has_param_query_traits T >
        [[nodiscard]] asrtl_status find(
            asrtr_param_query*                        q,
            asrtl_flat_id                             parent_id,
            char const*                               key,
            typename param_query_traits< T >::cb_type cb,
            void*                                     cb_ptr )
        {
                return query< T >( q, parent_id, key, cb, cb_ptr );
        }

        /// Raw find without explicit expected type and C callback + void* context.
        [[nodiscard]] asrtl_status find(
            asrtr_param_query* q,
            asrtl_flat_id      parent_id,
            char const*        key,
            asrtr_param_any_cb cb,
            void*              cb_ptr )
        {
                return query( q, parent_id, key, cb, cb_ptr );
        }

        template < has_param_query_traits T, typed_param_query_callable< T > CB >
        [[nodiscard]] asrtl_status query(
            asrtr_param_query* q,
            asrtl_flat_id      node_id,
            char const*        key,
            CB&                cb )
        {
                using traits             = param_query_traits< T >;
                q->expected_type         = traits::expected_type;
                q->cb.*traits::cb_member = []( asrtr_param_client*       c,
                                               asrtr_param_query*        qq,
                                               typename traits::raw_type raw ) {
                        ( *reinterpret_cast< CB* >( qq->cb_ptr ) )(
                            c, qq, static_cast< typename traits::value_type >( raw ) );
                };
                q->cb_ptr = &cb;
                return asrtr_param_client_query( q, &client_, node_id, key );
        }

        template < has_param_query_traits T >
        [[nodiscard]] asrtl_status query(
            asrtr_param_query*                        q,
            asrtl_flat_id                             node_id,
            char const*                               key,
            typename param_query_traits< T >::cb_type cb,
            void*                                     cb_ptr )
        {
                using traits             = param_query_traits< T >;
                q->expected_type         = traits::expected_type;
                q->cb.*traits::cb_member = cb;
                q->cb_ptr                = cb_ptr;
                return asrtr_param_client_query( q, &client_, node_id, key );
        }

        [[nodiscard]] asrtl_status query(
            asrtr_param_query* q,
            asrtl_flat_id      node_id,
            char const*        key,
            asrtr_param_any_cb cb,
            void*              cb_ptr )
        {
                q->expected_type = ASRTL_FLAT_VALUE_TYPE_NONE;
                q->cb.any        = cb;
                q->cb_ptr        = cb_ptr;
                return asrtr_param_client_query( q, &client_, node_id, key );
        }

        asrtl_status tick( uint32_t now = 0 )
        {
                return asrtr_param_client_tick( &client_, now );
        }

        ~param_client()
        {
                asrtr_param_client_deinit( &client_ );
        }

private:
        asrtr_param_client client_;
};

}  // namespace asrtr
