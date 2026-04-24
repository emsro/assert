#include "./util.hpp"

#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"

#include <limits>

namespace asrtio
{

bool _flat_tree_from_json_impl(
    asrtl_flat_tree&      tree,
    nlohmann::json const& j,
    asrt::flat_id         parent,
    char const*           key,
    asrt::flat_id&        next_id )
{
        asrt::flat_id const my_id = next_id++;

        std::string             str_storage;
        asrtl_flat_value_type   type   = ASRTL_FLAT_STYPE_NONE;
        union asrtl_flat_scalar scalar = {};
        switch ( j.type() ) {
        case nlohmann::json::value_t::null:
                type = ASRTL_FLAT_STYPE_NULL;
                break;
        case nlohmann::json::value_t::boolean:
                type            = ASRTL_FLAT_STYPE_BOOL;
                scalar.bool_val = j.get< bool >() ? 1u : 0u;
                break;
        case nlohmann::json::value_t::number_integer: {
                auto v = j.get< int64_t >();
                if ( v < 0 ) {
                        if ( v < std::numeric_limits< int32_t >::min() ) {
                                ASRTL_ERR_LOG(
                                    "asrtio", "integer %lld out of int32_t range", (long long) v );
                                return false;
                        }
                        type           = ASRTL_FLAT_STYPE_I32;
                        scalar.i32_val = static_cast< int32_t >( v );
                } else {
                        if ( v > std::numeric_limits< uint32_t >::max() ) {
                                ASRTL_ERR_LOG(
                                    "asrtio", "integer %lld out of uint32_t range", (long long) v );
                                return false;
                        }
                        type           = ASRTL_FLAT_STYPE_U32;
                        scalar.u32_val = static_cast< uint32_t >( v );
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
                type           = ASRTL_FLAT_STYPE_U32;
                scalar.u32_val = static_cast< uint32_t >( v );
                break;
        }
        case nlohmann::json::value_t::number_float: {
                auto v = j.get< double >();
                if ( v > static_cast< double >( std::numeric_limits< float >::max() ) ||
                     v < static_cast< double >( -std::numeric_limits< float >::max() ) ) {
                        ASRTL_ERR_LOG( "asrtio", "float value %f out of float range", v );
                        return false;
                }
                type             = ASRTL_FLAT_STYPE_FLOAT;
                scalar.float_val = static_cast< float >( v );
                break;
        }
        case nlohmann::json::value_t::string:
                str_storage    = j.get< std::string >();
                type           = ASRTL_FLAT_STYPE_STR;
                scalar.str_val = str_storage.c_str();
                break;
        case nlohmann::json::value_t::object:
                type = ASRTL_FLAT_CTYPE_OBJECT;
                break;
        case nlohmann::json::value_t::array:
                type = ASRTL_FLAT_CTYPE_ARRAY;
                break;
        default:
                type = ASRTL_FLAT_STYPE_NULL;
                break;
        }

        enum asrtl_status s;
        if ( type == ASRTL_FLAT_CTYPE_OBJECT || type == ASRTL_FLAT_CTYPE_ARRAY )
                s = asrtl_flat_tree_append_cont( &tree, parent, my_id, key, type );
        else
                s = asrtl_flat_tree_append_scalar( &tree, parent, my_id, key, type, scalar );
        if ( s != ASRTL_SUCCESS ) {
                ASRTL_ERR_LOG( "asrtio", "flat_tree_append failed: %s", asrtl_status_to_str( s ) );
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

bool _flat_tree_to_json_impl( asrtl_flat_tree& tree, asrt::flat_id node_id, nlohmann::json& out )
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
        case ASRTL_FLAT_STYPE_NULL:
                out = nullptr;
                break;
        case ASRTL_FLAT_STYPE_BOOL:
                out = res.value.data.s.bool_val != 0;
                break;
        case ASRTL_FLAT_STYPE_U32:
                out = res.value.data.s.u32_val;
                break;
        case ASRTL_FLAT_STYPE_I32:
                out = res.value.data.s.i32_val;
                break;
        case ASRTL_FLAT_STYPE_FLOAT:
                out = res.value.data.s.float_val;
                break;
        case ASRTL_FLAT_STYPE_STR:
                out = res.value.data.s.str_val;
                break;
        case ASRTL_FLAT_CTYPE_OBJECT: {
                out               = nlohmann::json::object();
                asrt::flat_id cid = res.value.data.cont.first_child;
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
        case ASRTL_FLAT_CTYPE_ARRAY: {
                out               = nlohmann::json::array();
                asrt::flat_id cid = res.value.data.cont.first_child;
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
