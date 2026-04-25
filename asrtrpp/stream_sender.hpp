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

#include "./stream.hpp"
#include "./task_unit.hpp"

namespace asrt
{

/// Sender: co_await define<Ts...>(client, schema_id)
/// Calls stream_client::define() and completes with a stream_schema<Ts...> by value
/// once the done_cb fires (via tick).
template < typename... Ts >
struct stream_define_sender
{
        using sender_concept        = ecor::sender_t;
        using completion_signatures = ecor::completion_signatures<
            ecor::set_value_t( stream_schema< Ts... > ),
            ecor::set_error_t( status ) >;

        asrtr_stream_client* client_;
        uint8_t              schema_id_;

        template < ecor::receiver R >
        struct op
        {
                asrtr_stream_client* client_;
                uint8_t              schema_id_;
                R                    recv;

                void start()
                {
                        auto cb = +[]( void* ptr, enum asrtl_status status ) {
                                auto& self = *static_cast< op* >( ptr );
                                if ( status == ASRTL_SUCCESS )
                                        self.recv.set_value( stream_schema< Ts... >{
                                            self.client_, self.schema_id_ } );
                                else
                                        self.recv.set_error( status );
                        };
                        auto s = define(
                            *client_,
                            schema_id_,
                            stream_schema< Ts... >::fields_,
                            sizeof...( Ts ),
                            { cb, this } );
                        if ( s != ASRTL_SUCCESS )
                                recv.set_error( s );
                }
        };

        template < ecor::receiver R >
        auto connect( R&& r ) && noexcept
        {
                return op< R >{ client_, schema_id_, (R&&) r };
        }
};

/// Define a stream schema.  Returns a sender that completes with a
/// stream_schema<Ts...> once the DEFINE message has been acknowledged.
template < typename... Ts >
ecor::sender auto define( asrtr_stream_client& client, uint8_t schema_id )
{
        return stream_define_sender< Ts... >{ &client, schema_id };
}


/// Sender: co_await emit(schema, args...)
/// Encodes args into a buffer, calls emit_raw, and completes once the done_cb fires.
template < typename... Ts >
struct stream_emit_sender
{
        using sender_concept = ecor::sender_t;
        using completion_signatures =
            ecor::completion_signatures< ecor::set_value_t(), ecor::set_error_t( status ) >;

        stream_schema< Ts... >* schema_;
        uint8_t                 buf_[stream_schema< Ts... >::emit_size];

        template < ecor::receiver R >
        struct op
        {
                stream_schema< Ts... >* schema_;
                uint8_t                 buf_[stream_schema< Ts... >::emit_size];
                R                       recv;

                void start()
                {
                        auto cb = +[]( void* ptr, enum asrtl_status status ) {
                                auto& self = *static_cast< op* >( ptr );
                                if ( status == ASRTL_SUCCESS )
                                        self.recv.set_value();
                                else
                                        self.recv.set_error( status );
                        };
                        auto st = schema_->emit_raw( buf_, { cb, this } );
                        if ( st != ASRTL_SUCCESS )
                                recv.set_error( st );
                }
        };

        template < ecor::receiver R >
        auto connect( R&& r ) && noexcept
        {
                op< R > o{ schema_, {}, (R&&) r };
                std::memcpy( o.buf_, buf_, sizeof( buf_ ) );
                return o;
        }
};

/// Emit one record.  Returns a sender that completes once the DATA message
/// has been sent.
template < typename... Ts >
ecor::sender auto emit( stream_schema< Ts... >& schema, Ts... args )
{
        stream_emit_sender< Ts... > s{ &schema, {} };
        uint8_t*                    p = s.buf_;
        ( strm_field_traits< Ts >::encode( p, args ), ... );
        return s;
}


}  // namespace asrt
