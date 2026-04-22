#pragma once

#include "../asrtc/diag.h"
#include "../asrtl/asrtl_assert.h"
#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"
#include "../asrtlpp/sender.hpp"
#include "../asrtlpp/util.hpp"

#include <memory>

namespace asrtc
{

using diag_record = asrtc_diag_record;

struct diag
{
        template < typename CB >
        diag( asrtl_node* prev, CB& send_cb, struct asrtl_allocator alloc )
        {
                if ( auto s = asrtc_diag_init( &diag_, prev, asrtl::make_sender( send_cb ), alloc );
                     s != ASRTL_SUCCESS ) {
                        ASRTL_ERR_LOG( "asrtc_diag", "init failed: %s", asrtl_status_to_str( s ) );
                        ASRTL_ASSERT( false );
                }
        }

        diag( diag&& )      = delete;
        diag( diag const& ) = delete;

        asrtl_node* node()
        {
                return &diag_.node;
        }

        struct record_deleter
        {
                struct asrtl_allocator* alloc;
                void                    operator()( asrtc_diag_record* rec ) const
                {
                        asrtc_diag_free_record( alloc, rec );
                }
        };
        using record_ptr = std::unique_ptr< asrtc_diag_record, record_deleter >;

        /// Returns the next pending diagnostic record, or nullptr if none.
        /// The returned record is freed automatically when the unique_ptr is destroyed.
        record_ptr take_record()
        {
                return { asrtc_diag_take_record( &diag_ ), record_deleter{ &diag_.alloc } };
        }

        ~diag()
        {
                if ( auto s = asrtc_diag_deinit( &diag_ ); s != ASRTL_SUCCESS )
                        ASRTL_ERR_LOG(
                            "asrtc_diag", "deinit failed: %s", asrtl_status_to_str( s ) );
        }

private:
        asrtc_diag diag_;
};

}  // namespace asrtc
