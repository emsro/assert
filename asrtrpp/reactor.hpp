#pragma once

#include "../asrtr/reactor.h"
#include "./util.hpp"

#include <span>

namespace asrtr
{

template < typename T >
using opt = std::optional< T >;

using status = asrtr_status;


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

        void add_test( asrtr_test* test )
        {
                asrtr_reactor_add_test( &reac, test );
        }

        ~reactor() = default;

private:
        asrtr_reactor reac;
};

}  // namespace asrtr
