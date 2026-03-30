#pragma once

#include "../asrtlpp/sender.hpp"
#include "../asrtlpp/util.hpp"
#include "../asrtr/reactor.h"

#include <span>

namespace asrtr
{

using status = asrtr_status;
using record = asrtr_record;

template < typename T >
struct unit
{
        template < typename... Args >
        unit( Args&&... args )
          : test( (Args&&) args... )
        {
                asrtr_test_init( &atest, test.name(), static_cast< unit* >( this ), unit::cb );
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

        T          test;
        asrtr_test atest;
};

struct reactor
{
        template < asrtl::sender_callable CB >
        reactor( CB& send_cb, char const* desc )
        {
                std::ignore = asrtr_reactor_init( &reac, asrtl::make_sender( send_cb ), desc );
        }

        reactor( asrtl_sender sender, char const* desc )
        {
                std::ignore = asrtr_reactor_init( &reac, sender, desc );
        }

        reactor( reactor&& )      = delete;
        reactor( reactor const& ) = delete;

        // XXX: reevaluate this
        asrtl_node* node()
        {
                return &reac.node;
        }

        status tick()
        {
                return asrtr_reactor_tick( &reac );
        }

        template < typename D >
        void add_test( D& test )
        {
                add_test( &test.atest );
        }

        void add_test( asrtr_test* test )
        {
                asrtr_reactor_add_test( &reac, test );
        }

        ~reactor() = default;

private:
        asrtr_reactor reac;
};

}  // namespace asrtr
