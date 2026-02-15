#pragma once

#include "../asrtc/status_to_str.h"
#include "../asrtcpp/controller.hpp"
#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"
#include "../asrtl/util.h"
#include "../asrtlpp/util.hpp"

#include <deque>
#include <functional>
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

        asrtl::status write( uv_stream_t* client, asrtl::chann_id id, std::span< uint8_t > buff )
        {
                uint8_t  buffer[1024];
                uint8_t* p  = buffer + 8;  // offset for COBS encoding
                uint8_t* pp = p;

                size_t size = sizeof( asrtl_chann_id ) + buff.size();
                asrtl_add_u16( &pp, id );
                memcpy( pp, buff.data(), buff.size() );
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
                    "asrtio",
                    "Sending to channel %u: %u bytes, total: %u",
                    id,
                    buff.size(),
                    sp.e - sp.b );

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

struct task
{
        enum res
        {
                finished,
                runnning
        };
        virtual res tick() = 0;
        virtual ~task()    = default;
};

struct uv_tasks
{
        uv_loop_t* loop;

        uv_idle_t                             idle_handle;
        std::deque< std::unique_ptr< task > > tasks;
        std::function< void() >               on_complete;

        uv_tasks( uv_loop_t* loop )
          : loop( loop )
        {
                idle_handle.data = this;
        }

        void push( std::unique_ptr< task > task )
        {
                tasks.push_back( std::move( task ) );
        }

        void set_on_complete( std::function< void() > cb )
        {
                on_complete = std::move( cb );
        }

        void start()
        {
                uv_idle_init( loop, &idle_handle );
                uv_idle_start( &idle_handle, []( uv_idle_t* h ) {
                        static_cast< uv_tasks* >( h->data )->on_idle();
                } );
        }

        void on_idle()
        {
                if ( !tasks.empty() ) {
                        auto& task = tasks.front();
                        if ( task->tick() == task::finished )
                                tasks.pop_front();
                } else if ( on_complete ) {
                        auto cb     = std::move( on_complete );
                        on_complete = nullptr;
                        cb();
                }
        }
};

struct after_idle : task
{
        asrtc::controller&      cntr;
        std::function< void() > cb;

        after_idle( asrtc::controller& cntr, std::function< void() > cb )
          : cntr( cntr )
          , cb( std::move( cb ) )
        {
        }

        res tick() override
        {
                if ( !cntr.is_idle() )
                        return runnning;
                cb();
                return finished;
        }
};

struct test_pool_task : task
{
        asrtc::controller& cntr;
        int32_t            count = -1;
        uint32_t           idx   = 0;

        std::function< void( uint32_t id, std::string_view name ) > cb;

        test_pool_task(
            asrtc::controller&                                          cntr,
            std::function< void( uint32_t id, std::string_view name ) > cb )
          : cntr( cntr )
          , cb( std::move( cb ) )
        {
        }


        task::res tick() override
        {
                if ( !cntr.is_idle() )
                        return runnning;
                if ( count == -1 ) {
                        auto s = cntr.query_test_count( [this]( uint32_t c ) {
                                this->count = static_cast< int32_t >( c );
                                ASRTL_INF_LOG(
                                    "asrtio_main",
                                    "Test count received: %u",
                                    static_cast< uint32_t >( c ) );
                                return ASRTC_SUCCESS;
                        } );
                        if ( s != ASRTC_SUCCESS ) {
                                ASRTL_ERR_LOG(
                                    "asrtio_main",
                                    "Failed to start query test count: %s",
                                    asrtc_status_to_str( s ) );
                        }
                } else if ( idx < count ) {
                        auto s = cntr.query_test_info( idx, [this]( std::string_view desc ) {
                                ASRTL_INF_LOG(
                                    "asrtio_main",
                                    "Test info received: %u -> %s",
                                    this->idx,
                                    desc.data() );
                                this->cb( this->idx, desc );
                                ++idx;
                                return ASRTC_SUCCESS;
                        } );
                        if ( s != ASRTC_SUCCESS ) {
                                ASRTL_ERR_LOG(
                                    "asrtio_main",
                                    "Failed to start query test info: %s",
                                    asrtc_status_to_str( s ) );
                        }
                }
                return idx == count ? finished : runnning;
        }
};

inline void for_each_test(
    uv_tasks&                                                   tasks,
    asrtc::controller&                                          cntr,
    std::function< void( uint32_t id, std::string_view name ) > cb )
{
        auto p = std::make_unique< test_pool_task >( cntr, std::move( cb ) );
        tasks.push( std::move( p ) );
}


}  // namespace asrtio
