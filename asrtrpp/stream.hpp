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
#include "../asrtl/util.h"
#include "../asrtlpp/callback.hpp"
#include "../asrtlpp/sender.hpp"
#include "../asrtlpp/status_sender.hpp"
#include "../asrtr/stream.h"

#include <cstdint>
#include <cstring>

namespace asrt
{

template < typename T >
struct strm_field_traits;

template <>
struct strm_field_traits< uint8_t >
{
        static constexpr auto tag  = ASRT_STRM_FIELD_U8;
        static constexpr auto size = 1;
        static void           encode( uint8_t*& p, uint8_t v ) { *p++ = v; }
};

template <>
struct strm_field_traits< uint16_t >
{
        static constexpr auto tag  = ASRT_STRM_FIELD_U16;
        static constexpr auto size = 2;
        static void           encode( uint8_t*& p, uint16_t v ) { asrt_add_u16( &p, v ); }
};

template <>
struct strm_field_traits< uint32_t >
{
        static constexpr auto tag  = ASRT_STRM_FIELD_U32;
        static constexpr auto size = 4;
        static void           encode( uint8_t*& p, uint32_t v ) { asrt_add_u32( &p, v ); }
};

template <>
struct strm_field_traits< int8_t >
{
        static constexpr auto tag  = ASRT_STRM_FIELD_I8;
        static constexpr auto size = 1;
        static void encode( uint8_t*& p, int8_t v ) { *p++ = static_cast< uint8_t >( v ); }
};

template <>
struct strm_field_traits< int16_t >
{
        static constexpr auto tag  = ASRT_STRM_FIELD_I16;
        static constexpr auto size = 2;
        static void           encode( uint8_t*& p, int16_t v )
        {
                asrt_add_u16( &p, static_cast< uint16_t >( v ) );
        }
};

template <>
struct strm_field_traits< int32_t >
{
        static constexpr auto tag  = ASRT_STRM_FIELD_I32;
        static constexpr auto size = 4;
        static void           encode( uint8_t*& p, int32_t v ) { asrt_add_i32( &p, v ); }
};

template <>
struct strm_field_traits< float >
{
        static constexpr auto tag  = ASRT_STRM_FIELD_FLOAT;
        static constexpr auto size = 4;
        static void           encode( uint8_t*& p, float v )
        {
                uint32_t u;
                std::memcpy( &u, &v, 4 );
                asrt_add_u32( &p, u );
        }
};

template <>
struct strm_field_traits< bool >
{
        static constexpr auto tag  = ASRT_STRM_FIELD_BOOL;
        static constexpr auto size = 1;
        static void           encode( uint8_t*& p, bool v ) { *p++ = v ? 1 : 0; }
};

inline status init( ref< asrtr_stream_client > client, asrt_node& prev, autosender s )
{
        return asrtr_stream_client_init( client, &prev, s );
}

inline status define(
    ref< asrtr_stream_client >         client,
    uint8_t                            schema_id,
    enum asrt_strm_field_type_e const* fields,
    uint8_t                            field_count,
    callback< asrtr_stream_done_cb >   done_cb )
{
        return asrtr_stream_client_define(
            client, schema_id, fields, field_count, done_cb.fn, done_cb.ptr );
}
inline status emit(
    ref< asrtr_stream_client >       client,
    uint8_t                          schema_id,
    uint8_t const*                   data,
    uint16_t                         data_size,
    callback< asrtr_stream_done_cb > done_cb )
{
        return asrtr_stream_client_emit(
            client, schema_id, data, data_size, done_cb.fn, done_cb.ptr );
}

inline status reset( ref< asrtr_stream_client > client )
{
        return asrtr_stream_client_reset( client );
}

inline void deinit( ref< asrtr_stream_client > client )
{
        asrtr_stream_client_deinit( client );
}

/// Compile-time typed stream schema.
///
/// Constructed by stream_define_sender after a successful DEFINE.  Provides
/// emit(args..., done_cb, done_ptr) which encodes field values into a
/// fixed-size buffer and sends a DATA message.
template < typename... Ts >
struct stream_schema
{
        static constexpr uint16_t emit_size = ( strm_field_traits< Ts >::size + ... );

        /// XXX: error handling in constructor is bad
        stream_schema(
            ref< asrtr_stream_client >       client,
            uint8_t                          schema_id,
            callback< asrtr_stream_done_cb > done_cb )
          : client_( client )
          , schema_id_( schema_id )
        {
                auto s = define( client_, schema_id_, fields_, sizeof...( Ts ), done_cb );
                if ( s != ASRT_SUCCESS ) {
                        ASRT_ERR_LOG( "asrtr_stream_schema", "define failed" );
                        ASRT_ASSERT( false );
                }
        }

        stream_schema( ref< asrtr_stream_client > c, uint8_t id )
          : client_( c )
          , schema_id_( id )
        {
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

        status emit( Ts... args, callback< asrtr_stream_done_cb > done_cb )
        {
                uint8_t  buf[emit_size];
                uint8_t* p = buf;
                ( strm_field_traits< Ts >::encode( p, args ), ... );
                return asrt::emit( client_, schema_id_, buf, emit_size, done_cb );
        }

        status emit_raw( uint8_t const* buf, callback< asrtr_stream_done_cb > done_cb )
        {
                return asrt::emit( client_, schema_id_, buf, emit_size, done_cb );
        }

        ~stream_schema() = default;

        static constexpr enum asrt_strm_field_type_e fields_[] = {
            strm_field_traits< Ts >::tag... };

private:
        asrtr_stream_client* client_;
        uint8_t              schema_id_;
};

}  // namespace asrt
