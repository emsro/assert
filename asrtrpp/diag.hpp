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
#include "../asrtlpp/callback.hpp"
#include "../asrtlpp/task.hpp"
#include "../asrtlpp/util.hpp"
#include "../asrtr/diag.h"

namespace asrt
{

using status = asrt_status;

/// Enqueue a diagnostic record from @p file:@p line with optional expression @p extra.
/// Calls @p done_cb when the send completes.
inline void rec_diag(
    ref< asrt_diag_client >              d,
    char const*                          file,
    uint32_t                             line,
    char const*                          extra,
    callback< asrt_diag_record_done_cb > done_cb )
{
        asrt_diag_client_record( d, file, line, extra, done_cb.fn, done_cb.ptr );
}

/// Initialise a diag client and link it into the channel chain after @p prev.
ASRT_NODISCARD inline status init( ref< asrt_diag_client > d, asrt_node& prev )
{
        return asrt_diag_client_init( d, &prev );
}

/// Unlink and release the diag client.
inline void deinit( ref< asrt_diag_client > d )
{
        asrt_diag_client_deinit( d );
}

/// Context for diag_rec_sender: enqueues a diagnostic RECORD message.
struct _diag_rec_ctx
{
        using completion_signatures =
            ecor::completion_signatures< ecor::set_value_t(), ecor::set_error_t( status ) >;

        asrt_diag_client* d;
        char const*       file;
        uint32_t          line;
        char const*       extra;

        template < typename OP >
        void start( OP& op )
        {
                asrt_diag_client_record(
                    d,
                    file,
                    line,
                    extra,
                    +[]( void* p, enum asrt_status s ) {
                            auto& o = *static_cast< OP* >( p );
                            if ( s == ASRT_SUCCESS )
                                    o.receiver.set_value();
                            else
                                    o.receiver.set_error( s );
                    },
                    &op );
        }
};

/// Sender backing co_await rec_diag(d, file, line, extra).
/// Completes with void once the RECORD message has been sent.
using diag_rec_sender = ecor::sender_from< _diag_rec_ctx >;

/// co_await rec_diag(d, file, line, extra) — enqueue a diagnostic record;
/// completes with void once the send completes.
inline ecor::sender auto rec_diag(
    ref< asrt_diag_client > d,
    char const*             file,
    uint32_t                line,
    char const*             extra )
{
        return diag_rec_sender{ { d, file, line, extra } };
}

#define ASRT_CO_REQUIRE( diag, rec, x )                                         \
        do {                                                                    \
                if ( !( x ) ) {                                                 \
                        ( rec )->state = ASRT_TEST_FAIL;                        \
                        co_await rec_diag( diag, ASRT_FILENAME, __LINE__, #x ); \
                        co_return;                                              \
                }                                                               \
        } while ( 0 )

#define ASRT_CO_CHECK( diag, rec, x )                                           \
        do {                                                                    \
                if ( !( x ) ) {                                                 \
                        ( rec )->state = ASRT_TEST_FAIL;                        \
                        co_await rec_diag( diag, ASRT_FILENAME, __LINE__, #x ); \
                }                                                               \
        } while ( 0 )

}  // namespace asrt
