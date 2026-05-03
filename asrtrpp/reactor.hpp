/// Permission to use, copy, modify, and/or distribute this software for any
/// purpose with or without fee is hereby granted.
///
/// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
/// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
/// AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
/// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
/// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
/// OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
/// PERFORMANCE OF THIS SOFTWARE.
#pragma once

#include "../asrtl/asrt_assert.h"
#include "../asrtl/log.h"
#include "../asrtl/status_to_str.h"
#include "../asrtlpp/util.hpp"
#include "../asrtr/reactor.h"

#include <span>

namespace asrt
{

using send_req_list = asrt_send_req_list;
using record        = asrt_record;

/// Test adaptor that wraps a synchronous callable T into an asrt_test.
/// T must provide name() -> char const* and operator()(record&) -> asrt_status.
/// The instance must remain at a stable address for the lifetime of the reactor.
template < typename T >
struct unit : asrt_test
{
        template < typename... Args >
        unit( Args&&... args )
          : test( (Args&&) args... )
        {
                asrt_test_init( this, test.name(), static_cast< unit* >( this ), unit::cb );
        }

        unit( unit&& )      = delete;
        unit( unit const& ) = delete;

        static asrt_status cb( record* rec )
        {
                auto*       self = static_cast< unit* >( rec->inpt->test_ptr );
                asrt_status st   = self->test( *rec );
                if ( st != ASRT_SUCCESS )
                        rec->state = ASRT_TEST_FAIL;
                return st;
        }

        T test;
};

/// Initialise a reactor and wire it to @p req_l for outgoing messages.
/// @p desc is a human-readable target description (borrowed).
ASRT_NODISCARD inline enum asrt_status init(
    ref< asrt_reactor > reac,
    send_req_list&      req_l,
    char const*         desc )
{
        return asrt_reactor_init( reac, &req_l, desc );
}
/// Append @p test to the reactor's test list.
ASRT_NODISCARD inline enum asrt_status add_test( ref< asrt_reactor > reac, asrt_test& test )
{
        return asrt_reactor_add_test( reac, &test );
}

/// Unlink the reactor from the channel chain and release its resources.
inline void deinit( ref< asrt_reactor > reac )
{
        asrt_reactor_deinit( reac );
}

}  // namespace asrt
