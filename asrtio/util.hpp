#pragma once

#include "../asrtc/status_to_str.h"
#include "../asrtc/test_result_to_str.h"
#include "../asrtcpp/controller.hpp"
#include "../asrtl/flat_tree.h"
#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"
#include "../asrtl/util.h"
#include "../asrtlpp/util.hpp"

#include <cstring>
#include <deque>
#include <functional>
#include <limits>
#include <nlohmann/json.hpp>
#include <span>
#include <uv.h>

namespace asrtio
{

struct cobs_node
{
        asrtl_node*                      node;
        asrtl_cobs_ibuffer               recv;
        uint8_t                          ibuffer[1024];
        std::function< void( ssize_t ) > on_error;

        asrtl::status write( uv_stream_t* client, asrtl::chann_id id, asrtl::rec_span& buff )
        {
                uint8_t  buffer[1024];
                uint8_t* p  = buffer + 8;  // offset for COBS encoding
                uint8_t* pp = p;

                size_t size = sizeof( asrtl_chann_id );
                asrtl_add_u16( &pp, id );
                for ( asrtl::rec_span* sp = &buff; sp != nullptr; sp = sp->next ) {
                        if ( size + ( sp->e - sp->b ) > sizeof( buffer ) - 8 ) {
                                ASRTL_ERR_LOG( "asrtio", "Buffer overflow in COBS encoding" );
                                return ASRTL_SEND_ERR;
                        }
                        memcpy( pp, sp->b, sp->e - sp->b );
                        pp += sp->e - sp->b;
                        size += sp->e - sp->b;
                }
                struct asrtl_span sp{ .b = buffer, .e = buffer + sizeof buffer };
                auto              s = asrtl_cobs_encode_buffer( { .b = p, .e = p + size }, &sp );
                if ( s != ASRTL_SUCCESS ) {
                        ASRTL_ERR_LOG(
                            "asrtio", "COBS encoding failed: %s", asrtl_status_to_str( s ) );
                        return ASRTL_SEND_ERR;
                }

                auto* data = new uint8_t[sp.e - sp.b];
                memcpy( data, sp.b, sp.e - sp.b );
                ASRTL_DBG_LOG(
                    "asrtio", "Sending to channel %u: %u bytes, total: %u", id, size, sp.e - sp.b );

                auto* req      = new uv_write_t{};
                req->data      = data;
                uv_buf_t wrbuf = uv_buf_init( (char*) data, sp.e - sp.b );
                uv_write( req, client, &wrbuf, 1, []( uv_write_t* req, int status ) {
                        if ( status ) {
                                ASRTL_ERR_LOG(
                                    "asrtio_main", "Error on write: %s", uv_strerror( status ) );
                        }

                        delete[] static_cast< uint8_t* >( req->data );
                        delete req;
                } );
                return ASRTL_SUCCESS;
        }

        void on_data( std::span< uint8_t > data )
        {
                struct asrtl_span sp{ .b = data.data(), .e = data.data() + data.size() };
                auto              s = asrtl_chann_cobs_dispatch( &recv, node, sp );
                if ( s != ASRTL_SUCCESS ) {
                        ASRTL_ERR_LOG(
                            "asrtio", "COBS dispatch failed: %s", asrtl_status_to_str( s ) );
                        on_error( s );
                }
        }

        void start(
            uv_stream_t*                     client,
            asrtl_node*                      node,
            std::function< void( ssize_t ) > on_error )
        {
                asrtl_cobs_ibuffer_init(
                    &recv, (struct asrtl_span) { .b = ibuffer, .e = ibuffer + sizeof ibuffer } );
                this->node     = node;
                this->on_error = std::move( on_error );
                client->data   = this;
                uv_read_start(
                    client,
                    []( uv_handle_t*, size_t suggested_size, uv_buf_t* buf ) {
                            buf->base = new char[suggested_size];
                            buf->len  = suggested_size;
                    },
                    []( uv_stream_t* h, ssize_t nread, uv_buf_t const* buf ) {
                            auto& self = *static_cast< cobs_node* >( h->data );
                            if ( nread == UV_EOF ) {
                                    ASRTL_DBG_LOG( "asrtio", "Connection closed" );
                                    self.on_error( nread );
                            } else if ( nread < 0 ) {
                                    ASRTL_ERR_LOG(
                                        "asrtio",
                                        "Read error: %s",
                                        uv_strerror( static_cast< int >( nread ) ) );
                                    self.on_error( nread );
                            } else {
                                    self.on_data(
                                        std::span< uint8_t >{
                                            (uint8_t*) buf->base, (std::size_t) nread } );
                            }
                            delete[] buf->base;
                    } );
        }
};


inline bool _flat_tree_from_json_impl(
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
                    "asrtio",
                    "flat_tree_append failed: %s",
                    asrtl_status_to_str( s ) );
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


inline bool flat_tree_from_json( asrtl_flat_tree& tree, nlohmann::json const& j )
{
        asrtl_flat_id next_id = 1;
        return _flat_tree_from_json_impl( tree, j, 0, nullptr, next_id );
}

}  // namespace asrtio
