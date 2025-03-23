#include "./controller.hpp"

#include "../asrtc/controller.h"
#include "../asrtc/default_allocator.h"

namespace asrtc
{
struct controller_impl
{
        asrtc_controller asc;
        asrtl::send_cb   scb;
        error_cb         ecb;

        desc_cb        des_cb;
        tc_cb          tc_cb;
        desc_cb        ti_cb;
        test_result_cb te_cb;
};
namespace
{

template < auto ERR, typename... Ts >
auto cimpl_do( auto& cb, Ts&&... args )
{
        if ( !cb )
                return ERR;
        auto st = cb( (Ts&&) args... );
        cb      = {};
        return st;
}

asrtl::status cimpl_send( void* ptr, asrtl::chann_id id, struct asrtl_span buff )
{
        auto*                  ci = reinterpret_cast< controller_impl* >( ptr );
        std::span< std::byte > sp{ (std::byte*) buff.b, (std::byte*) buff.e };
        if ( !ci->scb )
                return ASRTL_SEND_ERR;
        return ci->scb( id, sp );
}

asrtc::status cimpl_error( void* ptr, asrtl::source src, uint16_t ecode )
{
        auto* ci = reinterpret_cast< controller_impl* >( ptr );
        return cimpl_do< ASRTC_CNTR_CB_ERR >( ci->ecb, src, ecode );
}

asrtc::status cimpl_desc( void* ptr, char* data )
{
        auto* ci = reinterpret_cast< controller_impl* >( ptr );
        return cimpl_do< ASRTC_CNTR_CB_ERR >( ci->des_cb, std::string_view{ data } );
}

asrtc::status cimpl_test_count( void* ptr, uint16_t count )
{
        auto* ci = reinterpret_cast< controller_impl* >( ptr );
        return cimpl_do< ASRTC_CNTR_CB_ERR >( ci->tc_cb, count );
}

asrtc::status cimpl_test_info( void* ptr, char* data )
{
        auto* ci = reinterpret_cast< controller_impl* >( ptr );
        return cimpl_do< ASRTC_CNTR_CB_ERR >( ci->ti_cb, data );
}

asrtc::status cimpl_test_result( void* ptr, struct asrtc_result* res )
{
        auto* ci = reinterpret_cast< controller_impl* >( ptr );
        return cimpl_do< ASRTC_CNTR_CB_ERR >( ci->te_cb, *res );
}

}  // namespace

controller::controller( asrtl::send_cb scb, error_cb ecb )
  : _impl{ new controller_impl{
        .scb = std::move( scb ),
        .ecb = std::move( ecb ),
    } }
{
        auto st = asrtc_cntr_init(
            &_impl->asc,
            { .ptr = _impl.get(), .cb = &cimpl_send },
            asrtc_default_allocator(),
            { .ptr = _impl.get(), .cb = &cimpl_error } );
        assert( st == ASRTC_SUCCESS );
}

controller::controller( controller&& ) = default;
controller::~controller()              = default;

asrtl_node* controller::node()
{
        return &_impl->asc.node;
}

asrtc::status controller::tick()
{
        return asrtc_cntr_tick( &_impl->asc );
}

bool controller::is_idle() const
{
        return asrtc_cntr_idle( &_impl->asc ) > 0;
}

asrtc::status controller::query_desc( desc_cb cb )
{
        auto st = asrtc_cntr_desc( &_impl->asc, &cimpl_desc, _impl.get() );
        if ( st == ASRTC_SUCCESS )
                _impl->des_cb = std::move( cb );
        return st;
}

asrtc::status controller::query_test_count( tc_cb cb )
{
        auto st = asrtc_cntr_test_count( &_impl->asc, &cimpl_test_count, _impl.get() );
        if ( st == ASRTC_SUCCESS )
                _impl->tc_cb = std::move( cb );
        return st;
}

asrtc::status controller::query_test_info( uint16_t id, desc_cb cb )
{
        auto st = asrtc_cntr_test_info( &_impl->asc, id, &cimpl_test_info, _impl.get() );
        if ( st == ASRTC_SUCCESS )
                _impl->ti_cb = std::move( cb );
        return st;
}

asrtc::status controller::exec_test( uint16_t id, test_result_cb cb )
{
        auto st = asrtc_cntr_test_exec( &_impl->asc, id, &cimpl_test_result, _impl.get() );
        if ( st == ASRTC_SUCCESS )
                _impl->te_cb = std::move( cb );
        return st;
}

}  // namespace asrtc
