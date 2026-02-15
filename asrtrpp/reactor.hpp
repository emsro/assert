#pragma once

#include "../asrtlpp/sender.hpp"
#include "../asrtlpp/util.hpp"
#include "../asrtr/reactor.h"

#include <span>

namespace asrtr
{

using status = asrtr_status;
using record = asrtr_record;

template < typename D >
struct unit
{
        unit()
        {
                asrtr_test_init( &atest, D::desc, static_cast< D* >( this ), D::cb );
        }

        unit( unit&& )      = delete;
        unit( unit const& ) = delete;

        static asrtr::status cb( record* rec )
        {
                // XXX: how to make this async?
                auto*         d  = static_cast< D* >( rec->test_ptr );
                asrtr::status st = ( *d )();
                rec->state       = st == ASRTR_SUCCESS ? ASRTR_TEST_PASS : ASRTR_TEST_FAIL;
                return st;
        }

        asrtr_test atest;
};

struct reactor
{
        template < typename CB >
        reactor( CB& cb, char const* desc )
        {
                std::ignore = asrtr_reactor_init( &reac, asrtl::make_sender( cb ), desc );
        }

        reactor( reactor&& )      = delete;
        reactor( reactor const& ) = delete;

        // XXX: reevaluate this
        asrtl_node* node()
        {
                return &reac.node;
        }

        status tick( std::span< uint8_t > buff )
        {
                return asrtr_reactor_tick( &reac, asrtl::cnv( buff ) );
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
