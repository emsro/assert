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

#include "./param.hpp"
#include "./task_unit.hpp"

namespace asrt
{

template < has_param_query_traits T >
struct param_result
{
        using value_type = typename param_query_traits< T >::value_type;

        value_type  value;
        char const* key;  // Points to internal buffer of param client, valid until next query
        flat_id     next_sibling;

        operator value_type() const { return value; }
};


template < has_param_query_traits T >
struct param_query_sender
{
        using sender_concept = ecor::sender_t;

        using completion_signatures = ecor::completion_signatures<
            ecor::set_value_t( param_result< T > ),
            ecor::set_error_t( status ) >;

        asrtr_param_client* client_;
        char const*         key;
        flat_id             node_id;

        template < ecor::receiver R >
        struct op
        {
                asrtr_param_query   q;
                asrtr_param_client* c;
                char const*         key;
                flat_id             node_id;
                R                   recv;

                op( R&& r, asrtr_param_client* client, char const* k, flat_id id )
                  : c( client )
                  , key( k )
                  , node_id( id )
                  , recv( (R&&) r )
                {
                }

                void start()
                {
                        using traits = param_query_traits< T >;
                        auto cb      = +[]( asrtr_param_client*,
                                       asrtr_param_query*        q,
                                       typename traits::raw_type raw ) {
                                auto& self = *reinterpret_cast< op* >( q->cb_ptr );
                                if ( q->error_code != ASRT_PARAM_ERR_NONE )
                                        self.recv.set_error( ASRT_RECV_ERR );
                                else
                                        self.recv.set_value( param_result< T >{
                                            static_cast< typename traits::value_type >( raw ),
                                            q->key,
                                            q->next_sibling } );
                        };
                        auto s = query< T >( c, &q, node_id, key, cb, this );
                        if ( s != ASRT_SUCCESS )
                                recv.set_error( ASRT_RECV_ERR );
                }
        };


        template < ecor::receiver R >
        auto connect( R&& r ) && noexcept
        {
                return op< R >{ (R&&) r, client_, key, node_id };
        }
};

template < typename T >
ecor::sender auto fetch( ref< asrtr_param_client > client, flat_id node_id )
{
        return param_query_sender< T >{ client, nullptr, node_id };
}

template < typename T >
ecor::sender auto find( ref< asrtr_param_client > client, flat_id parent_id, char const* key )
{
        return param_query_sender< T >{ client, key, parent_id };
}


}  // namespace asrt
