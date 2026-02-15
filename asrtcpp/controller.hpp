
#pragma once

#include "../asrtc/callbacks.h"
#include "../asrtc/result.h"
#include "../asrtc/status.h"
#include "../asrtlpp/util.hpp"

#include <functional>

namespace asrtc
{
using asrtl::uptr;
using status         = asrtc_status;
using result         = asrtc_result;
using error_cb       = std::function< status( asrtl::source, asrtl::ecode ) >;
using desc_cb        = std::function< status( std::string_view ) >;
using tc_cb          = std::function< status( uint32_t ) >;
using test_result_cb = std::function< status( result const& ) >;

struct controller_impl;

struct controller
{
        controller( asrtl::send_cb scb, error_cb ecb );
        controller( controller&& );

        [[nodiscard]]
        asrtc::status tick();

        // XXX: reevaluate this
        asrtl_node* node();

        bool is_idle() const;

        [[nodiscard]] asrtc::status query_desc( desc_cb cb );
        [[nodiscard]] asrtc::status query_test_count( tc_cb cb );
        [[nodiscard]] asrtc::status query_test_info( uint16_t id, desc_cb cb );
        [[nodiscard]] asrtc::status exec_test( uint16_t id, test_result_cb cb );

        ~controller();

private:
        uptr< controller_impl > _impl;
};

}  // namespace asrtc
