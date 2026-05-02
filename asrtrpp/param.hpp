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

#include "../asrtl/asrt_assert.h"
#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"
#include "../asrtlpp/flat_type_traits.hpp"
#include "../asrtr/param.h"

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace asrt
{

using status = asrt_status;

// Trait system to pick C-API callback based on requested type
template < typename T >
struct param_query_traits;

template <>
struct param_query_traits< uint32_t > : flat_type_traits< uint32_t >
{
        using cb_type                   = asrt_param_u32_cb;
        static constexpr auto cb_member = &asrt_param_cb::u32;
};

template <>
struct param_query_traits< int32_t > : flat_type_traits< int32_t >
{
        using cb_type                   = asrt_param_i32_cb;
        static constexpr auto cb_member = &asrt_param_cb::i32;
};

template <>
struct param_query_traits< float > : flat_type_traits< float >
{
        using cb_type                   = asrt_param_float_cb;
        static constexpr auto cb_member = &asrt_param_cb::flt;
};

template <>
struct param_query_traits< char const* > : flat_type_traits< char const* >
{
        using cb_type                   = asrt_param_str_cb;
        static constexpr auto cb_member = &asrt_param_cb::str;
};

template <>
struct param_query_traits< bool > : flat_type_traits< bool >
{
        using cb_type                   = asrt_param_bool_cb;
        static constexpr auto cb_member = &asrt_param_cb::bln;
};

template <>
struct param_query_traits< obj > : flat_type_traits< obj >
{
        using cb_type                   = asrt_param_obj_cb;
        static constexpr auto cb_member = &asrt_param_cb::obj;
};

template <>
struct param_query_traits< arr > : flat_type_traits< arr >
{
        using cb_type                   = asrt_param_arr_cb;
        static constexpr auto cb_member = &asrt_param_cb::arr;
};

template <>
struct param_query_traits< asrt_flat_value >
{
        using raw_type                  = asrt_flat_value;
        using value_type                = asrt_flat_value;
        using cb_type                   = asrt_param_any_cb;
        static constexpr auto flat_type = ASRT_FLAT_STYPE_NONE;
        static constexpr auto cb_member = &asrt_param_cb::any;
};

// Convenience aliases
template <>
struct param_query_traits< void > : param_query_traits< asrt_flat_value >
{
};

template < typename T >
concept has_param_query_traits = requires { typename param_query_traits< T >; };

template < typename CB, typename T >
concept typed_param_query_callable =
    has_param_query_traits< T > && requires(
                                       CB                                           cb,
                                       asrt_param_client*                           c,
                                       asrt_param_query*                            q,
                                       typename param_query_traits< T >::value_type v ) {
            { cb( c, q, v ) } -> std::same_as< void >;
    };

inline status init(
    ref< asrt_param_client > client,
    asrt_node&               prev,
    asrt_span                msg_buffer,
    uint32_t                 timeout )
{
        return asrt_param_client_init( client, &prev, msg_buffer, timeout );
}

[[nodiscard]] inline bool ready( ref< asrt_param_client const > client )
{
        return client->ready != 0;
}

[[nodiscard]] inline bool query_pending( ref< asrt_param_client const > client )
{
        return asrt_param_query_pending( client ) != 0;
}

[[nodiscard]] inline flat_id root_id( ref< asrt_param_client const > client )
{
        return asrt_param_client_root_id( client );
}

template < has_param_query_traits T, typed_param_query_callable< T > CB >
[[nodiscard]] asrt_status query(
    ref< asrt_param_client > cl,
    asrt_param_query*        q,
    flat_id                  node_id,
    char const*              key,
    CB&                      cb )
{
        using traits     = param_query_traits< T >;
        q->expected_type = traits::flat_type;
        q->cb.*traits::cb_member =
            []( asrt_param_client* c, asrt_param_query* qq, typename traits::raw_type raw ) {
                    ( *reinterpret_cast< CB* >( qq->cb_ptr ) )(
                        c, qq, static_cast< typename traits::value_type >( raw ) );
            };
        q->cb_ptr = &cb;
        return asrt_param_client_query( q, cl, node_id, key );
}

template < has_param_query_traits T >
[[nodiscard]] asrt_status query(
    ref< asrt_param_client >                  cl,
    asrt_param_query*                         q,
    flat_id                                   node_id,
    char const*                               key,
    typename param_query_traits< T >::cb_type cb,
    void*                                     cb_ptr )
{
        using traits             = param_query_traits< T >;
        q->expected_type         = traits::flat_type;
        q->cb.*traits::cb_member = cb;
        q->cb_ptr                = cb_ptr;
        return asrt_param_client_query( q, cl, node_id, key );
}

[[nodiscard]] inline asrt_status query(
    ref< asrt_param_client > cl,
    asrt_param_query*        q,
    flat_id                  node_id,
    char const*              key,
    asrt_param_any_cb        cb,
    void*                    cb_ptr )
{
        q->expected_type = ASRT_FLAT_STYPE_NONE;
        q->cb.any        = cb;
        q->cb_ptr        = cb_ptr;
        return asrt_param_client_query( q, cl, node_id, key );
}

/// Fetch information about node node_id from param client, call cb with the value if
/// successful. cb will be called even on error, with an appropriate error code in
/// q->error_code and possibly an invalid value. T is the expected type of the value, if
/// value type doesn't match, cb will be called with an error.
template < has_param_query_traits T, typed_param_query_callable< T > CB >
[[nodiscard]] asrt_status fetch(
    ref< asrt_param_client > cl,
    asrt_param_query*        q,
    flat_id                  node_id,
    CB&                      cb )
{
        return query< T >( cl, q, node_id, nullptr, cb );
}

/// Fetch information about node node_id from param client, call cb with the value if
/// successful. This overload expects raw function pointer and void* context.
template < has_param_query_traits T >
[[nodiscard]] asrt_status fetch(
    ref< asrt_param_client >                  cl,
    asrt_param_query*                         q,
    flat_id                                   node_id,
    typename param_query_traits< T >::cb_type cb,
    void*                                     cb_ptr )
{
        return query< T >( cl, q, node_id, nullptr, cb, cb_ptr );
}

/// Raw fetch without explicit expected type and C callback + void* context.
[[nodiscard]] inline asrt_status fetch(
    ref< asrt_param_client > cl,
    asrt_param_query*        q,
    flat_id                  node_id,
    asrt_param_any_cb        cb,
    void*                    cb_ptr )
{
        return query( cl, q, node_id, nullptr, cb, cb_ptr );
}

/// Find child with key under parent_id, call cb with the value if successful.
template < has_param_query_traits T, typed_param_query_callable< T > CB >
[[nodiscard]] asrt_status find(
    ref< asrt_param_client > cl,
    asrt_param_query*        q,
    flat_id                  parent_id,
    char const*              key,
    CB&                      cb )
{
        return query< T >( cl, q, parent_id, key, cb );
}

/// Find child with key under parent_id. Raw function pointer + void* context overload.
template < has_param_query_traits T >
[[nodiscard]] asrt_status find(
    ref< asrt_param_client >                  cl,
    asrt_param_query*                         q,
    flat_id                                   parent_id,
    char const*                               key,
    typename param_query_traits< T >::cb_type cb,
    void*                                     cb_ptr )
{
        return query< T >( cl, q, parent_id, key, cb, cb_ptr );
}

/// Raw find without explicit expected type and C callback + void* context.
[[nodiscard]] inline asrt_status find(
    ref< asrt_param_client > cl,
    asrt_param_query*        q,
    flat_id                  parent_id,
    char const*              key,
    asrt_param_any_cb        cb,
    void*                    cb_ptr )
{
        return query( cl, q, parent_id, key, cb, cb_ptr );
}

inline void deinit( ref< asrt_param_client > client )
{
        asrt_param_client_deinit( client );
}

}  // namespace asrt
