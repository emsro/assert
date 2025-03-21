#include "./controller.hpp"

#include "../asrtc/controller.h"
#include "../asrtc/default_allocator.h"

namespace asrtc
{
struct controller_impl
{
        asrtc_controller asc;
        asrtl::sender_cb scb;
        error_cb         ecb;

        desc_cb desc_cb;
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
        return cimpl_do< ASRTL_SEND_ERR >( ci->scb, id, sp );
}

asrtc::status cimpl_error( void* ptr, asrtc::source src, uint16_t ecode )
{
        auto* ci = reinterpret_cast< controller_impl* >( ptr );
        return cimpl_do< ASRTC_CNTR_CB_ERR >( ci->ecb, src, ecode );
}

asrtc::status cimpl_desc( void* ptr, char* data )
{
        auto* ci = reinterpret_cast< controller_impl* >( ptr );
        return cimpl_do< ASRTC_CNTR_CB_ERR >( ci->desc_cb, std::string_view{ data } );
}
}  // namespace

controller::controller( uptr< controller_impl > impl )
  : _impl( std::move( impl ) )
{
}

controller::controller( controller&& ) = default;
controller::~controller()              = default;

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
        if ( _impl->desc_cb )
                return ASRTC_CNTR_BUSY_ERR;
        _impl->desc_cb = std::move( cb );
        auto st        = asrtc_cntr_desc( &_impl->asc, &cimpl_desc, _impl.get() );
        if ( st != ASRTC_SUCCESS )
                _impl->desc_cb = {};
        return st;
}

opt< controller > make_controller( asrtl::sender_cb scb, error_cb ecb )
{
        uptr< controller_impl > impl{
            new controller_impl{ .scb = std::move( scb ), .ecb = std::move( ecb ) } };
        auto st = asrtc_cntr_init(
            &impl->asc,
            { .ptr = impl.get(), .cb = &cimpl_send },
            asrtc_default_allocator(),
            { .ptr = impl.get(), .cb = &cimpl_error } );
        if ( st == ASRTC_SUCCESS )
                return controller{ std::move( impl ) };
        return {};
}

}  // namespace asrtc
