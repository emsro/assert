
#pragma once

#include "../asrtc/callbacks.h"
#include "../asrtc/result.h"
#include "../asrtc/status.h"
#include "../asrtl/chann.h"

#include <functional>
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
using status         = asrtc_status;
using source         = asrtc_source;
using result         = asrtc_result;
using error_cb       = std::function< status( source, asrtl::ecode ) >;
using desc_cb        = std::function< status( std::string_view ) >;
using tc_cb          = std::function< status( uint32_t ) >;
using test_result_cb = std::function< status( result const& ) >;
template < typename T >
using uptr = std::unique_ptr< T >;
template < typename T >
using opt = std::optional< T >;

struct controller_impl;

struct controller
{
        controller( uptr< controller_impl > impl );
        controller( controller&& );

        asrtc::status tick();

        // XXX: reevaluate this
        asrtl_node* node();

        bool is_idle() const;

        asrtc::status query_desc( desc_cb cb );
        asrtc::status query_test_count( tc_cb cb );
        asrtc::status query_test_info( uint16_t id, desc_cb cb );
        asrtc::status exec_test( uint16_t id, test_result_cb cb );

        ~controller();

private:
        uptr< controller_impl > _impl;
};

opt< controller > make_controller( asrtl::sender_cb scb, error_cb ecb );

}  // namespace asrtc
