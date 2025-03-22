#pragma once

#include "../asrtr/reactor.h"
#include "./util.hpp"

#include <span>

namespace asrtr
{

template < typename T >
using opt = std::optional< T >;

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
                auto*         d  = static_cast< D* >( rec->test_ptr );
                asrtr::status st = ( *d )();
                rec->state       = st == ASRTR_SUCCESS ? ASRTR_TEST_PASS : ASRTR_TEST_FAIL;
                return st;
        }

        asrtr_test atest;
};

struct reactor
{
        template < send_cb CB >
        reactor( CB& cb, char const* desc )
        {
                std::ignore = asrtr_reactor_init( &reac, make_sender( cb ), desc );
        }

        reactor( reactor&& )      = delete;
        reactor( reactor const& ) = delete;

        // XXX: reevaluate this
        asrtl_node* node()
        {
                return &reac.node;
        }

        status tick( std::span< std::byte > buff )
        {
                return asrtr_reactor_tick( &reac, cnv( buff ) );
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
