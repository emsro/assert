#pragma once

#include "../asrtl/flat_tree.h"

#include <cstdint>

namespace asrtl
{

using flat_id = asrtl_flat_id;

// Shared tag types for object/array containers
struct obj
{
};
struct arr
{
};

// Base trait system mapping C++ types to flat_tree value metadata.
// Each specialisation exposes:
//   raw_type   — the type stored in the C union (e.g. uint32_t for bool)
//   value_type — the C++ facing type
//   flat_type  — the asrtl_flat_value_type constant
template < typename T >
struct flat_type_traits;

template <>
struct flat_type_traits< uint32_t >
{
        using raw_type                  = uint32_t;
        using value_type                = uint32_t;
        static constexpr auto flat_type = ASRTL_FLAT_STYPE_U32;
        static constexpr bool is_scalar = true;
};

template <>
struct flat_type_traits< int32_t >
{
        using raw_type                  = int32_t;
        using value_type                = int32_t;
        static constexpr auto flat_type = ASRTL_FLAT_STYPE_I32;
        static constexpr bool is_scalar = true;
};

template <>
struct flat_type_traits< float >
{
        using raw_type                  = float;
        using value_type                = float;
        static constexpr auto flat_type = ASRTL_FLAT_STYPE_FLOAT;
        static constexpr bool is_scalar = true;
};

template <>
struct flat_type_traits< char const* >
{
        using raw_type                  = char const*;
        using value_type                = char const*;
        static constexpr auto flat_type = ASRTL_FLAT_STYPE_STR;
        static constexpr bool is_scalar = true;
};

template <>
struct flat_type_traits< bool >
{
        using raw_type                  = uint32_t;
        using value_type                = bool;
        static constexpr auto flat_type = ASRTL_FLAT_STYPE_BOOL;
        static constexpr bool is_scalar = true;
};

template <>
struct flat_type_traits< obj >
{
        using raw_type                  = asrtl_flat_child_list;
        using value_type                = asrtl_flat_child_list;
        static constexpr auto flat_type = ASRTL_FLAT_CTYPE_OBJECT;
        static constexpr bool is_scalar = false;
};

template <>
struct flat_type_traits< arr >
{
        using raw_type                  = asrtl_flat_child_list;
        using value_type                = asrtl_flat_child_list;
        static constexpr auto flat_type = ASRTL_FLAT_CTYPE_ARRAY;
        static constexpr bool is_scalar = false;
};

}  // namespace asrtl
