#pragma once

#include "./param.hpp"
#include "./task_unit.hpp"

namespace asrtr
{

template < has_param_query_traits T >
struct param_result
{
        using value_type = typename param_query_traits< T >::value_type;

        value_type        value;
        char const*       key;  // Points to internal buffer of param client, valid until next query
        asrtl::flat_id    next_sibling;

        operator value_type() const
        {
                return value;
        }
};


template < has_param_query_traits T >
struct param_query_sender
{
        using sender_concept = ecor::sender_t;

        using completion_signatures = ecor::completion_signatures<
            ecor::set_value_t( param_result< T > ),
            ecor::set_error_t( task_error ) >;

        param_client* client_;
        char const*   key;
        asrtl::flat_id node_id;

        template < ecor::receiver R >
        struct op
        {
                asrtr_param_query q;
                param_client*     c;
                char const*       key;
                asrtl::flat_id    node_id;
                R                 recv;

                op( R&& r, param_client* client, char const* k, asrtl::flat_id id )
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
                                if ( q->error_code != ASRTL_PARAM_ERR_NONE )
                                        self.recv.set_error( task_error::test_fail );
                                else
                                        self.recv.set_value(
                                            param_result< T >{
                                                static_cast< typename traits::value_type >( raw ),
                                                q->key,
                                                q->next_sibling } );
                        };
                        auto s = c->query< T >( &q, node_id, key, cb, this );
                        if ( s != ASRTL_SUCCESS )
                                recv.set_error( task_error::test_fail );
                }
        };


        template < ecor::receiver R >
        auto connect( R&& r ) && noexcept
        {
                return op< R >{ (R&&) r, client_, key, node_id };
        }
};

template < typename T >
ecor::sender auto fetch( param_client& client, asrtl::flat_id node_id )
{
        return param_query_sender< T >{ &client, nullptr, node_id };
}

template < typename T >
ecor::sender auto find( param_client& client, asrtl::flat_id parent_id, char const* key )
{
        return param_query_sender< T >{ &client, key, parent_id };
}


}  // namespace asrtr
