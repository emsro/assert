#include "./util.hpp"

#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"

#include <limits>

namespace asrtio
{

bool _flat_tree_from_json_impl(
    asrtl_flat_tree&      tree,
    nlohmann::json const& j,
    asrtl_flat_id         parent,
    char const*           key,
    asrtl_flat_id&        next_id )
{
        asrtl_flat_id const my_id = next_id++;

        asrtl_flat_value val{};
        switch ( j.type() ) {
        case nlohmann::json::value_t::null:
                val = asrtl_flat_value_null();
                break;
        case nlohmann::json::value_t::boolean:
                val = asrtl_flat_value_bool( j.get< bool >() ? 1u : 0u );
                break;
        case nlohmann::json::value_t::number_integer: {
                auto v = j.get< int64_t >();
                if ( v < 0 ) {
                        if ( v < std::numeric_limits< int32_t >::min() ) {
                                ASRTL_ERR_LOG(
                                    "asrtio", "integer %lld out of int32_t range", (long long) v );
                                return false;
                        }
                        val = asrtl_flat_value_i32( static_cast< int32_t >( v ) );
                } else {
                        if ( v > std::numeric_limits< uint32_t >::max() ) {
                                ASRTL_ERR_LOG(
                                    "asrtio", "integer %lld out of uint32_t range", (long long) v );
                                return false;
                        }
                        val = asrtl_flat_value_u32( static_cast< uint32_t >( v ) );
                }
                break;
        }
        case nlohmann::json::value_t::number_unsigned: {
                auto v = j.get< uint64_t >();
                if ( v > std::numeric_limits< uint32_t >::max() ) {
                        ASRTL_ERR_LOG(
                            "asrtio",
                            "unsigned integer %llu out of uint32_t range",
                            (unsigned long long) v );
                        return false;
                }
                val = asrtl_flat_value_u32( static_cast< uint32_t >( v ) );
                break;
        }
        case nlohmann::json::value_t::number_float: {
                auto v = j.get< double >();
                if ( v > static_cast< double >( std::numeric_limits< float >::max() ) ||
                     v < static_cast< double >( -std::numeric_limits< float >::max() ) ) {
                        ASRTL_ERR_LOG( "asrtio", "float value %f out of float range", v );
                        return false;
                }
                val = asrtl_flat_value_float( static_cast< float >( v ) );
                break;
        }
        case nlohmann::json::value_t::string:
                val = asrtl_flat_value_str( j.get< std::string >().c_str() );
                break;
        case nlohmann::json::value_t::object:
                val = asrtl_flat_value_object();
                break;
        case nlohmann::json::value_t::array:
                val = asrtl_flat_value_array();
                break;
        default:
                val = asrtl_flat_value_null();
                break;
        }

        auto s = asrtl_flat_tree_append( &tree, parent, my_id, key, val );
        if ( s != ASRTL_SUCCESS ) {
                ASRTL_ERR_LOG(
                    "asrtio", "flat_tree_append failed: %s", asrtl_status_to_str( s ) );
                return false;
        }

        if ( j.is_object() ) {
                for ( auto const& [k, v] : j.items() )
                        if ( !_flat_tree_from_json_impl( tree, v, my_id, k.c_str(), next_id ) )
                                return false;
        } else if ( j.is_array() ) {
                for ( auto const& elem : j )
                        if ( !_flat_tree_from_json_impl( tree, elem, my_id, nullptr, next_id ) )
                                return false;
        }

        return true;
}

bool _flat_tree_to_json_impl(
    asrtl_flat_tree& tree,
    asrtl_flat_id    node_id,
    nlohmann::json&  out )
{
        asrtl_flat_query_result res{};
        auto                    s = asrtl_flat_tree_query( &tree, node_id, &res );
        if ( s != ASRTL_SUCCESS ) {
                ASRTL_ERR_LOG(
                    "asrtio",
                    "flat_tree_to_json: query failed for node %u: %s",
                    node_id,
                    asrtl_status_to_str( s ) );
                return false;
        }

        switch ( res.value.type ) {
        case ASRTL_FLAT_VALUE_TYPE_NULL:
                out = nullptr;
                break;
        case ASRTL_FLAT_VALUE_TYPE_BOOL:
                out = res.value.bool_val != 0;
                break;
        case ASRTL_FLAT_VALUE_TYPE_U32:
                out = res.value.u32_val;
                break;
        case ASRTL_FLAT_VALUE_TYPE_I32:
                out = res.value.i32_val;
                break;
        case ASRTL_FLAT_VALUE_TYPE_FLOAT:
                out = res.value.float_val;
                break;
        case ASRTL_FLAT_VALUE_TYPE_STR:
                out = res.value.str_val;
                break;
        case ASRTL_FLAT_VALUE_TYPE_OBJECT: {
                out               = nlohmann::json::object();
                asrtl_flat_id cid = res.value.obj_val.first_child;
                while ( cid != 0 ) {
                        asrtl_flat_query_result cr{};
                        s = asrtl_flat_tree_query( &tree, cid, &cr );
                        if ( s != ASRTL_SUCCESS ) {
                                ASRTL_ERR_LOG(
                                    "asrtio",
                                    "flat_tree_to_json: query failed for child %u: %s",
                                    cid,
                                    asrtl_status_to_str( s ) );
                                return false;
                        }
                        nlohmann::json cj;
                        if ( !_flat_tree_to_json_impl( tree, cid, cj ) )
                                return false;
                        out[cr.key] = std::move( cj );
                        cid         = cr.next_sibling;
                }
                break;
        }
        case ASRTL_FLAT_VALUE_TYPE_ARRAY: {
                out               = nlohmann::json::array();
                asrtl_flat_id cid = res.value.arr_val.first_child;
                while ( cid != 0 ) {
                        asrtl_flat_query_result cr{};
                        s = asrtl_flat_tree_query( &tree, cid, &cr );
                        if ( s != ASRTL_SUCCESS ) {
                                ASRTL_ERR_LOG(
                                    "asrtio",
                                    "flat_tree_to_json: query failed for child %u: %s",
                                    cid,
                                    asrtl_status_to_str( s ) );
                                return false;
                        }
                        nlohmann::json cj;
                        if ( !_flat_tree_to_json_impl( tree, cid, cj ) )
                                return false;
                        out.push_back( std::move( cj ) );
                        cid = cr.next_sibling;
                }
                break;
        }
        default:
                ASRTL_ERR_LOG(
                    "asrtio",
                    "flat_tree_to_json: unknown value type %d for node %u",
                    res.value.type,
                    node_id );
                return false;
        }

        return true;
}

}  // namespace asrtio
