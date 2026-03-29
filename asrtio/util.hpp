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

struct clock
{
        virtual std::chrono::milliseconds now() const = 0;
};

struct steady_clock : clock
{
        std::chrono::milliseconds now() const override
        {
                return std::chrono::duration_cast< std::chrono::milliseconds >(
                    std::chrono::steady_clock::now().time_since_epoch() );
        }
};

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


bool _flat_tree_from_json_impl(
    asrtl_flat_tree&      tree,
    nlohmann::json const& j,
    asrtl_flat_id         parent,
    char const*           key,
    asrtl_flat_id&        next_id );

inline bool flat_tree_from_json(
    asrtl_flat_tree&      tree,
    nlohmann::json const& j,
    asrtl_flat_id&        next_id )
{
        return _flat_tree_from_json_impl( tree, j, 0, nullptr, next_id );
}


bool _flat_tree_to_json_impl( asrtl_flat_tree& tree, asrtl_flat_id node_id, nlohmann::json& out );

inline bool flat_tree_to_json( asrtl_flat_tree& tree, nlohmann::json& out )
{
        return _flat_tree_to_json_impl( tree, 1, out );
}

}  // namespace asrtio
