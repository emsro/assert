#pragma once

#include "../asrtl/asrtl_assert.h"
#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"
#include "../asrtlpp/sender.hpp"
#include "../asrtr/diag.h"

#include <cstdint>

namespace asrt
{

void rec_diag( ref< asrtr_diag > d, char const* file, uint32_t line, char const* extra = nullptr )
{
        asrtr_diag_record( d, file, line, extra );
}

inline status init( ref< asrtr_diag > d, asrtl_node& prev, autosender sender )
{
        return asrtr_diag_init( d, &prev, sender );
}

inline void deinit( ref< asrtr_diag > d )
{
        asrtr_diag_deinit( d );
}

}  // namespace asrt
