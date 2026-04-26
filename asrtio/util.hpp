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

#include "../asrtc/test_result_to_str.h"
#include "../asrtcpp/controller.hpp"
#include "../asrtl/flat_tree.h"
#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"
#include "../asrtl/util.h"
#include "../asrtlpp/flat_type_traits.hpp"
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
        asrt_node*                       node;
        asrt_cobs_ibuffer                recv;
        uint8_t                          ibuffer[4096];
        char const*                      module = "asrtio";
        std::function< void( ssize_t ) > on_error;

        asrt::status write( uv_stream_t* client, asrt::chann_id id, asrt::rec_span& buff ) const
        {
                uint8_t  buffer[1024];
                uint8_t* p  = buffer + 8;  // offset for COBS encoding
                uint8_t* pp = p;

                size_t size = sizeof( asrt::chann_id );
                asrt_add_u16( &pp, id );
                for ( asrt::rec_span* sp = &buff; sp != nullptr; sp = sp->next ) {
                        if ( size + ( sp->e - sp->b ) > sizeof( buffer ) - 8 ) {
                                ASRT_ERR_LOG( module, "Buffer overflow in COBS encoding" );
                                return ASRT_SEND_ERR;
                        }
                        memcpy( pp, sp->b, sp->e - sp->b );
                        pp += sp->e - sp->b;
                        size += sp->e - sp->b;
                }
                struct asrt_span sp
                {
                        .b = buffer, .e = buffer + sizeof buffer
                };
                auto s = asrt_cobs_encode_buffer( { .b = p, .e = p + size }, &sp );
                if ( s != ASRT_SUCCESS ) {
                        ASRT_ERR_LOG( module, "COBS encoding failed: %s", asrt_status_to_str( s ) );
                        return ASRT_SEND_ERR;
                }

                auto* data = new uint8_t[sp.e - sp.b];
                memcpy( data, sp.b, sp.e - sp.b );
                ASRT_DBG_LOG(
                    module, "Sending to channel %u: %u bytes, total: %u", id, size, sp.e - sp.b );

                auto* req      = new uv_write_t{};
                req->data      = data;
                uv_buf_t wrbuf = uv_buf_init( (char*) data, sp.e - sp.b );
                uv_write( req, client, &wrbuf, 1, []( uv_write_t* req, int status ) {
                        if ( status ) {
                                ASRT_ERR_LOG(
                                    "asrtio_main", "Error on write: %s", uv_strerror( status ) );
                        }

                        delete[] static_cast< uint8_t* >( req->data );
                        delete req;
                } );
                return ASRT_SUCCESS;
        }

        void on_data( std::span< uint8_t > data )
        {
                struct asrt_span sp
                {
                        .b = data.data(), .e = data.data() + data.size()
                };
                auto s = asrt_chann_cobs_dispatch( &recv, node, sp );
                if ( s != ASRT_SUCCESS ) {
                        ASRT_ERR_LOG( module, "COBS dispatch failed: %s", asrt_status_to_str( s ) );
                        on_error( UV_UNKNOWN );
                }
        }

        void start(
            uv_stream_t*                     client,
            asrt_node*                       node,
            char const*                      mod,
            std::function< void( ssize_t ) > on_error )
        {
                asrt_cobs_ibuffer_init(
                    &recv, ( struct asrt_span ){ .b = ibuffer, .e = ibuffer + sizeof ibuffer } );
                this->node     = node;
                this->module   = mod;
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
                                    ASRT_DBG_LOG( self.module, "Connection closed" );
                                    self.on_error( nread );
                            } else if ( nread < 0 ) {
                                    ASRT_ERR_LOG(
                                        self.module,
                                        "Read error: %s",
                                        uv_strerror( static_cast< int >( nread ) ) );
                                    self.on_error( nread );
                            } else {
                                    self.on_data( std::span< uint8_t >{
                                        (uint8_t*) buf->base, (std::size_t) nread } );
                            }
                            delete[] buf->base;
                    } );
        }
};


bool flat_tree_from_json( asrt_flat_tree& tree, nlohmann::json const& j, asrt::flat_id& next_id );

bool flat_tree_to_json( asrt_flat_tree& tree, nlohmann::json& out );

bool flat_tree_to_json( asrt_flat_tree& tree, asrt::flat_id node_id, nlohmann::json& out );

}  // namespace asrtio
