#pragma once

#include "../asrtc/diag.h"
#include "../asrtl/asrtl_assert.h"
#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"
#include "../asrtlpp/sender.hpp"
#include "../asrtlpp/util.hpp"

#include <memory>

namespace asrt
{

using diag_record = asrtc_diag_record;

struct diag_record_deleter
{
        struct asrtl_allocator* alloc;
        void operator()( asrtc_diag_record* rec ) const { asrtc_diag_free_record( alloc, rec ); }
};

inline status init(
    ref< asrtc_diag > d,
    asrtl_node&       prev,
    autosender        sender,
    asrtl_allocator   alloc )
{
        return asrtc_diag_init( d, &prev, sender, alloc );
}

inline std::unique_ptr< diag_record, diag_record_deleter > take_record( ref< asrtc_diag > d )
{
        return { asrtc_diag_take_record( d ), diag_record_deleter{ &d->alloc } };
}

inline void deinit( ref< asrtc_diag > d )
{
        asrtc_diag_deinit( d );
}


}  // namespace asrt
