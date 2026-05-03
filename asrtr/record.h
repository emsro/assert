
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
#ifndef ASRT_RECORD_H
#define ASRT_RECORD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../asrtl/status.h"

#include <stdint.h>

/// Lifecycle state of a test execution.
enum asrt_test_state
{
        ASRT_TEST_INIT    = 1,  ///< Test was not yet run.
        ASRT_TEST_RUNNING = 2,  ///< Test is still executing.
        ASRT_TEST_ERROR   = 3,  ///< Error in the test infrastructure.
        ASRT_TEST_FAIL    = 4,  ///< Test assertion failed.
        ASRT_TEST_PASS    = 5,  ///< Test passed.
};

struct asrt_record;
typedef enum asrt_status ( *asrt_test_callback )( struct asrt_record* );

/// Per-invocation context forwarded to the test entry point.
struct asrt_test_input
{
        void*              test_ptr;    ///< Context pointer registered with asrt_test_init().
        asrt_test_callback continue_f;  ///< Entry point to call on each tick.
        uint32_t           run_id;      ///< Unique run token from the controller.
};

/// Mutable test state passed to the test entry point on every tick.
struct asrt_record
{
        enum asrt_test_state state;  ///< Current state; updated by assertions and by the reactor.

        struct asrt_test_input const* inpt;  ///< Immutable per-invocation context.
};

/// Mark the record as failed.  Used internally by ASRT_CHECK / ASRT_REQUIRE.
void asrt_fail( struct asrt_record* rec );

/// Check @p x; if false, mark the record as failed (non-fatal: execution continues).
#define ASRT_RECORD_CHECK( rec, x )           \
        do {                                  \
                if ( !( x ) )                 \
                        asrt_fail( ( rec ) ); \
        } while ( 0 )

/// Check @p x; if false, mark the record as failed and return immediately (fatal).
#define ASRT_RECORD_REQUIRE( rec, x )         \
        do {                                  \
                if ( !( x ) ) {               \
                        asrt_fail( ( rec ) ); \
                        return ASRT_SUCCESS;  \
                }                             \
        } while ( 0 )

#ifdef __cplusplus
}
#endif

#endif
