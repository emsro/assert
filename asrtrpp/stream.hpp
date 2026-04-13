#pragma once

#include "../asrtl/asrtl_assert.h"
#include "../asrtl/log.h"
#include "../asrtl/util.h"
#include "../asrtlpp/callback.hpp"
#include "../asrtlpp/sender.hpp"
#include "../asrtr/status_to_str.h"
#include "../asrtr/stream.h"

#include <cstdint>
#include <cstring>

namespace asrtr
{

template < typename T >
struct strm_field_traits;

template <>
struct strm_field_traits< uint8_t >
{
        static constexpr auto tag  = ASRTL_STRM_FIELD_U8;
        static constexpr auto size = 1;
        static void           encode( uint8_t*& p, uint8_t v )
        {
                *p++ = v;
        }
};

template <>
struct strm_field_traits< uint16_t >
{
        static constexpr auto tag  = ASRTL_STRM_FIELD_U16;
        static constexpr auto size = 2;
        static void           encode( uint8_t*& p, uint16_t v )
        {
                asrtl_add_u16( &p, v );
        }
};

template <>
struct strm_field_traits< uint32_t >
{
        static constexpr auto tag  = ASRTL_STRM_FIELD_U32;
        static constexpr auto size = 4;
        static void           encode( uint8_t*& p, uint32_t v )
        {
                asrtl_add_u32( &p, v );
        }
};

template <>
struct strm_field_traits< int8_t >
{
        static constexpr auto tag  = ASRTL_STRM_FIELD_I8;
        static constexpr auto size = 1;
        static void           encode( uint8_t*& p, int8_t v )
        {
                *p++ = static_cast< uint8_t >( v );
        }
};

template <>
struct strm_field_traits< int16_t >
{
        static constexpr auto tag  = ASRTL_STRM_FIELD_I16;
        static constexpr auto size = 2;
        static void           encode( uint8_t*& p, int16_t v )
        {
                asrtl_add_u16( &p, static_cast< uint16_t >( v ) );
        }
};

template <>
struct strm_field_traits< int32_t >
{
        static constexpr auto tag  = ASRTL_STRM_FIELD_I32;
        static constexpr auto size = 4;
        static void           encode( uint8_t*& p, int32_t v )
        {
                asrtl_add_i32( &p, v );
        }
};

template <>
struct strm_field_traits< float >
{
        static constexpr auto tag  = ASRTL_STRM_FIELD_FLOAT;
        static constexpr auto size = 4;
        static void           encode( uint8_t*& p, float v )
        {
                uint32_t u;
                std::memcpy( &u, &v, 4 );
                asrtl_add_u32( &p, u );
        }
};

template <>
struct strm_field_traits< bool >
{
        static constexpr auto tag  = ASRTL_STRM_FIELD_BOOL;
        static constexpr auto size = 1;
        static void           encode( uint8_t*& p, bool v )
        {
                *p++ = v ? 1 : 0;
        }
};


/// Reactor-side stream client.
///
/// Provides define(), emit(), tick(), and reset().  All operations are
/// asynchronous: define/emit enqueue a message and fire a done_cb via tick().
struct stream_client
{
        template < typename CB >
        stream_client( asrtl_node* prev, CB& send_cb )
        {
                if ( auto s =
                         asrtr_stream_client_init( &client_, prev, asrtl::make_sender( send_cb ) );
                     s != ASRTR_SUCCESS ) {
                        ASRTL_ERR_LOG(
                            "asrtr_stream", "init failed: %s", asrtr_status_to_str( s ) );
                        ASRTL_ASSERT( false );
                }
        }

        stream_client( stream_client&& )      = delete;
        stream_client( stream_client const& ) = delete;

        asrtl_node* node()
        {
                return &client_.node;
        }

        asrtr_status define(
            uint8_t                             schema_id,
            enum asrtl_strm_field_type_e const* fields,
            uint8_t                             field_count,
            asrtl::callback< asrtr_stream_done_cb > done_cb )
        {
                return asrtr_stream_client_define(
                    &client_, schema_id, fields, field_count, done_cb.fn, done_cb.ptr );
        }

        asrtr_status tick()
        {
                return asrtr_stream_client_tick( &client_ );
        }

        asrtr_status emit(
            uint8_t              schema_id,
            uint8_t const*       buf,
            uint16_t             size,
            asrtl::callback< asrtr_stream_done_cb > done_cb )
        {
                return asrtr_stream_client_emit(
                    &client_, schema_id, buf, size, done_cb.fn, done_cb.ptr );
        }

        asrtr_status reset()
        {
                return asrtr_stream_client_reset( &client_ );
        }

        ~stream_client() = default;

private:
        asrtr_stream_client client_;
};

/// Compile-time typed stream schema.
///
/// Constructed by stream_define_sender after a successful DEFINE.  Provides
/// emit(args..., done_cb, done_ptr) which encodes field values into a
/// fixed-size buffer and sends a DATA message.
template < typename... Ts >
struct stream_schema
{
        static constexpr uint16_t emit_size = ( strm_field_traits< Ts >::size + ... );

        stream_schema(
            stream_client&       client,
            uint8_t              schema_id,
            asrtl::callback< asrtr_stream_done_cb > done_cb )
          : client_( &client )
          , schema_id_( schema_id )
        {
                auto s = client_->define( schema_id_, fields_, sizeof...( Ts ), done_cb );
                if ( s != ASRTR_SUCCESS ) {
                        ASRTL_ERR_LOG( "asrtr_stream_schema", "define failed" );
                        ASRTL_ASSERT( false );
                }
        }

        stream_schema( stream_schema const& )            = delete;
        stream_schema& operator=( stream_schema const& ) = delete;

        stream_schema( stream_schema&& o ) noexcept
          : client_( o.client_ )
          , schema_id_( o.schema_id_ )
        {
                o.client_ = nullptr;
        }

        stream_schema& operator=( stream_schema&& o ) noexcept
        {
                client_    = o.client_;
                schema_id_ = o.schema_id_;
                o.client_  = nullptr;
                return *this;
        }

        asrtr_status emit( Ts... args, asrtl::callback< asrtr_stream_done_cb > done_cb )
        {
                uint8_t  buf[emit_size];
                uint8_t* p = buf;
                ( strm_field_traits< Ts >::encode( p, args ), ... );
                return client_->emit( schema_id_, buf, emit_size, done_cb );
        }

        asrtr_status emit_raw( uint8_t const* buf, asrtl::callback< asrtr_stream_done_cb > done_cb )
        {
                return client_->emit( schema_id_, buf, emit_size, done_cb );
        }

        stream_schema( stream_client* c, uint8_t id )
          : client_( c )
          , schema_id_( id )
        {
        }

        ~stream_schema() = default;

        static constexpr enum asrtl_strm_field_type_e fields_[] = {
            strm_field_traits< Ts >::tag... };

private:
        stream_client* client_;
        uint8_t        schema_id_;
};

}  // namespace asrtr
