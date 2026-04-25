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
#include "../asrtlpp/sender.hpp"
#include "../asrtlpp/util.hpp"
#include "../asrtr/reactor.h"

#include <span>

namespace asrt
{

using record = asrtr_record;

// Test harness that stores test T which should be callable with a record reference and return a
// status.
template < typename T >
struct unit : asrtr_test
{
        template < typename... Args >
        unit( Args&&... args )
          : test( (Args&&) args... )
        {
                asrtr_test_init( this, test.name(), static_cast< unit* >( this ), unit::cb );
        }

        unit( unit&& )      = delete;
        unit( unit const& ) = delete;

        static asrt_status cb( record* rec )
        {
                auto*       self = static_cast< unit* >( rec->inpt->test_ptr );
                asrt_status st   = self->test( *rec );
                if ( st != ASRT_SUCCESS )
                        rec->state = ASRTR_TEST_FAIL;
                return st;
        }

        T test;
};

/// XXX: add C++ init to other asrt:: abstractions, inluding deinit
inline enum asrt_status init( ref< asrtr_reactor > reac, autosender sender, char const* desc )
{
        return asrtr_reactor_init( reac, sender, desc );
}

inline enum asrt_status add_test( ref< asrtr_reactor > reac, asrtr_test& test )
{
        return asrtr_reactor_add_test( reac, &test );
}

inline void deinit( ref< asrtr_reactor > reac )
{
        asrtr_reactor_deinit( reac );
}

}  // namespace asrt
