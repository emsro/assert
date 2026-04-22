
#pragma once

#include "../asrtc/callbacks.h"
#include "../asrtc/result.h"
#include "../asrtlpp/sender.hpp"
#include "../asrtlpp/util.hpp"

#include <functional>

namespace asrtc
{
using asrtl::uptr;
using status         = asrtl_status;
using result         = asrtc_result;
using error_cb       = std::function< status( asrtl::source, asrtl::ecode ) >;
using init_cb        = std::function< asrtl::status( status ) >;
using desc_cb        = std::function< asrtl::status( status, std::string_view ) >;
using tc_cb          = std::function< asrtl::status( status, uint32_t ) >;
using test_info_cb   = std::function< asrtl::status( status, uint16_t, std::string_view ) >;
using test_result_cb = std::function< asrtl::status( status, result const& ) >;

struct controller_impl;

struct controller
{
        controller( asrtl::sender sender, asrtl::allocator alloc, error_cb ecb );

        controller( controller&& );

        [[nodiscard]]
        asrtc::status start( init_cb icb, uint32_t timeout );

        // XXX: reevaluate this
        asrtl_node* node();

        bool is_idle() const;

        [[nodiscard]] asrtc::status query_desc( desc_cb cb, uint32_t timeout );
        [[nodiscard]] asrtc::status query_test_count( tc_cb cb, uint32_t timeout );
        [[nodiscard]] asrtc::status query_test_info(
            uint16_t     id,
            test_info_cb cb,
            uint32_t     timeout );
        [[nodiscard]] asrtc::status exec_test( uint16_t id, test_result_cb cb, uint32_t timeout );

        ~controller();

private:
        uptr< controller_impl > _impl;
};

}  // namespace asrtc
