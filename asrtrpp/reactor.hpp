#pragma once

#include "../asrtl/asrtl_assert.h"
#include "../asrtl/log.h"
#include "../asrtlpp/sender.hpp"
#include "../asrtlpp/util.hpp"
#include "../asrtr/reactor.h"
#include "../asrtr/status_to_str.h"

#include <span>

namespace asrtr
{

using status = asrtr_status;
using record = asrtr_record;

// Test harness that stores test T which should be callable with a record reference and return a
// status.
template < typename T >
struct unit : asrtr_test
{
        template < typename... Args >
        unit( Args&&... args )
          : test( (Args&&) args... )
        {
                asrtr_test_init( this, test.name(), static_cast< unit* >( this ), unit::cb );
        }

        unit( unit&& )      = delete;
        unit( unit const& ) = delete;

        static asrtr::status cb( record* rec )
        {
                auto*         self = static_cast< unit* >( rec->inpt->test_ptr );
                asrtr::status st   = self->test( *rec );
                if ( st != ASRTR_SUCCESS )
                        rec->state = ASRTR_TEST_FAIL;
                return st;
        }

        T test;
};

struct reactor
{
        template < asrtl::sender_callable CB >
        reactor( CB& send_cb, char const* desc )
        {
                if ( auto s = asrtr_reactor_init( &reac, asrtl::make_sender( send_cb ), desc );
                     s != ASRTR_SUCCESS ) {
                        ASRTL_ERR_LOG(
                            "asrtr_reactor", "init failed: %s", asrtr_status_to_str( s ) );
                        ASRTL_ASSERT( false );
                }
        }

        reactor( asrtl_sender sender, char const* desc )
        {
                if ( auto s = asrtr_reactor_init( &reac, sender, desc ); s != ASRTR_SUCCESS ) {
                        ASRTL_ERR_LOG(
                            "asrtr_reactor", "init failed: %s", asrtr_status_to_str( s ) );
                        ASRTL_ASSERT( false );
                }
        }

        reactor( reactor&& )      = delete;
        reactor( reactor const& ) = delete;

        asrtl_node* node()
        {
                return &reac.node;
        }

        void add_test( asrtr_test& test )
        {
                asrtr_reactor_add_test( &reac, &test );
        }

        ~reactor() = default;

private:
        asrtr_reactor reac;
};

}  // namespace asrtr
