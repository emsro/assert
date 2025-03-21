
#pragma once

#include "../asrtc/callbacks.h"
#include "../asrtc/status.h"
#include "../asrtl/chann.h"

#include <memory>
#include <span>
#include <utility>

namespace asrtl
{
using chann_id  = asrtl_chann_id;
using status    = asrtl_status;
using sender_cb = std::function< asrtl_status( chann_id, std::span< std::byte > ) >;
using ecode     = uint16_t;
}  // namespace asrtl


namespace asrtc
{
using status   = asrtc_status;
using source   = asrtc_source;
using error_cb = std::function< status( source, asrtl::ecode ) >;
using desc_cb  = std::function< status( std::string_view ) >;
template < typename T >
using uptr = std::unique_ptr< T >;
template < typename T >
using opt = std::optional< T >;

struct controller_impl;

struct controller
{
        controller( uptr< controller_impl > impl );
        controller( controller&& );

        asrtc_status tick();

        bool is_idle() const;

        asrtc_status query_desc( desc_cb cb );

        ~controller();

private:
        uptr< controller_impl > _impl;
};

opt< controller > make_controller( asrtl::sender_cb scb, error_cb ecb );

}  // namespace asrtc
