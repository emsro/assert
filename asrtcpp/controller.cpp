#include "./controller.hpp"

#include "../asrtc/controller.h"
#include "../asrtc/default_allocator.h"
#include "../asrtl/asrtl_assert.h"
#include "../asrtl/log.h"

namespace asrtc
{
struct controller_impl
{
        asrtc_controller asc;
        error_cb         ecb;
        init_cb          ini_cb;

        desc_cb        des_cb;
        tc_cb          test_count_cb;
        test_info_cb   ti_cb;
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

asrtl::status cimpl_init( void* ptr, asrtc::status s )
{
        auto* ci = reinterpret_cast< controller_impl* >( ptr );
        return cimpl_do< ASRTL_CALLBACK_ERR >( ci->ini_cb, s );
}

asrtc::status cimpl_error( void* ptr, asrtl::source src, uint16_t ecode )
{
        auto* ci = reinterpret_cast< controller_impl* >( ptr );
        return cimpl_do< ASRTL_CALLBACK_ERR >( ci->ecb, src, ecode );
}

asrtl::status cimpl_desc( void* ptr, asrtc::status s, char* data )
{
        auto* ci = reinterpret_cast< controller_impl* >( ptr );
        return cimpl_do< ASRTL_CALLBACK_ERR >( ci->des_cb, s, std::string_view{ data } );
}

asrtl::status cimpl_test_count( void* ptr, asrtc::status s, uint16_t count )
{
        auto* ci = reinterpret_cast< controller_impl* >( ptr );
        return cimpl_do< ASRTL_CALLBACK_ERR >( ci->test_count_cb, s, count );
}

asrtl::status cimpl_test_info( void* ptr, asrtc::status s, uint16_t tid, char* data )
{
        auto* ci = reinterpret_cast< controller_impl* >( ptr );
        return cimpl_do< ASRTL_CALLBACK_ERR >( ci->ti_cb, s, tid, std::string_view{ data } );
}

asrtl::status cimpl_test_result( void* ptr, asrtc::status s, struct asrtc_result* res )
{
        auto* ci = reinterpret_cast< controller_impl* >( ptr );
        return cimpl_do< ASRTL_CALLBACK_ERR >( ci->te_cb, s, *res );
}

}  // namespace

controller::controller( asrtl::sender sender, asrtl::allocator alloc, error_cb ecb )
  : _impl{ new controller_impl{
        .ecb = std::move( ecb ),
    } }
{
        auto st = asrtc_cntr_init(
            &_impl->asc, sender, alloc, { .ptr = _impl.get(), .cb = &cimpl_error } );
        ASRTL_ASSERT( st == ASRTL_SUCCESS );
}

asrtc::status controller::start( init_cb icb, uint32_t timeout )
{
        _impl->ini_cb = std::move( icb );
        return asrtc_cntr_start( &_impl->asc, &cimpl_init, _impl.get(), timeout );
}

controller::controller( controller&& ) = default;

controller::~controller()
{
        if ( _impl )
                asrtc_cntr_deinit( &_impl->asc );
}

asrtl_node* controller::node()
{
        return &_impl->asc.node;
}

bool controller::is_idle() const
{
        return asrtc_cntr_idle( &_impl->asc ) > 0;
}

asrtc::status controller::query_desc( desc_cb cb, uint32_t timeout )
{
        auto st = asrtc_cntr_desc( &_impl->asc, &cimpl_desc, _impl.get(), timeout );
        if ( st == ASRTL_SUCCESS )
                _impl->des_cb = std::move( cb );
        else
                ASRTL_ERR_LOG(
                    "asrtcpp_controller",
                    "Query description failed: %s",
                    asrtl_status_to_str( st ) );
        return st;
}

asrtc::status controller::query_test_count( tc_cb cb, uint32_t timeout )
{
        auto st = asrtc_cntr_test_count( &_impl->asc, &cimpl_test_count, _impl.get(), timeout );
        if ( st == ASRTL_SUCCESS )
                _impl->test_count_cb = std::move( cb );
        else
                ASRTL_ERR_LOG(
                    "asrtcpp_controller",
                    "Query test count failed: %s",
                    asrtl_status_to_str( st ) );
        return st;
}

asrtc::status controller::query_test_info( uint16_t id, test_info_cb cb, uint32_t timeout )
{
        auto st = asrtc_cntr_test_info( &_impl->asc, id, &cimpl_test_info, _impl.get(), timeout );
        if ( st == ASRTL_SUCCESS )
                _impl->ti_cb = std::move( cb );
        else
                ASRTL_ERR_LOG(
                    "asrtcpp_controller", "Query test info failed: %s", asrtl_status_to_str( st ) );
        return st;
}

asrtc::status controller::exec_test( uint16_t id, test_result_cb cb, uint32_t timeout )
{
        auto st = asrtc_cntr_test_exec( &_impl->asc, id, &cimpl_test_result, _impl.get(), timeout );
        if ( st == ASRTL_SUCCESS )
                _impl->te_cb = std::move( cb );
        else
                ASRTL_ERR_LOG(
                    "asrtcpp_controller", "Execute test failed: %s", asrtl_status_to_str( st ) );
        return st;
}

}  // namespace asrtc
