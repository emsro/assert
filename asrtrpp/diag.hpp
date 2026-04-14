#pragma once

#include "../asrtl/asrtl_assert.h"
#include "../asrtl/log.h"
#include "../asrtlpp/sender.hpp"
#include "../asrtr/diag.h"
#include "../asrtr/status_to_str.h"

#include <cstdint>

namespace asrtr
{

struct diag
{
        template < typename CB >
        diag( asrtl_node* prev, CB& send_cb )
        {
                if ( auto s = asrtr_diag_init( &diag_, prev, asrtl::make_sender( send_cb ) );
                     s != ASRTR_SUCCESS ) {
                        ASRTL_ERR_LOG(
                            "asrtr_diag", "init failed: %s", asrtr_status_to_str( s ) );
                        ASRTL_ASSERT( false );
                }
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

        ~diag()
        {
                asrtr_diag_deinit( &diag_ );
        }

private:
        asrtr_diag diag_;
};

}  // namespace asrtr
