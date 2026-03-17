#pragma once

#include "../asrtc/diag.h"
#include "../asrtlpp/sender.hpp"
#include "../asrtlpp/util.hpp"

#include <memory>

namespace asrtc
{

struct diag
{
        template < typename CB >
        diag(
            asrtl_node*            prev,
            CB&                    send_cb,
            struct asrtl_allocator alloc = asrtl_default_allocator() )
        {
                std::ignore = asrtc_diag_init( &diag_, prev, asrtl::make_sender( send_cb ), alloc );
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
                std::ignore = asrtc_diag_deinit( &diag_ );
        }

private:
        asrtc_diag diag_;
};

}  // namespace asrtc
