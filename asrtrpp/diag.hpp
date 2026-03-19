#pragma once

#include "../asrtlpp/sender.hpp"
#include "../asrtr/diag.h"

#include <cstdint>

namespace asrtr
{

struct diag
{
        template < typename CB >
        diag( asrtl_node* prev, CB& send_cb )
        {
                std::ignore = asrtr_diag_init( &diag_, prev, asrtl::make_sender( send_cb ) );
        }

        diag( diag&& )      = delete;
        diag( diag const& ) = delete;

        asrtl_node* node()
        {
                return &diag_.node;
        }

        void record( char const* file, uint32_t line, char const* extra = nullptr )
        {
                asrtr_diag_record( &diag_, file, line, extra );
        }

        ~diag() = default;

private:
        asrtr_diag diag_;
};

}  // namespace asrtr
