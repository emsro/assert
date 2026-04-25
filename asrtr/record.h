
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

enum asrt_test_state
{
        ASRT_TEST_INIT    = 1,  // Test was not yet run
        ASRT_TEST_RUNNING = 2,  // Test is still running
        ASRT_TEST_ERROR   = 3,  // Error in test suite
        ASRT_TEST_FAIL    = 4,  // Test did not pass
        ASRT_TEST_PASS    = 5,  // Test succeeded
};

struct asrt_record;
typedef enum asrt_status ( *asrt_test_callback )( struct asrt_record* );

struct asrt_test_input
{
        void*              test_ptr;
        asrt_test_callback continue_f;
        uint32_t           run_id;
};

struct asrt_record
{
        enum asrt_test_state state;  // Current state of the test

        struct asrt_test_input const* inpt;
};

void asrt_fail( struct asrt_record* rec );

#define ASRT_RECORD_CHECK( rec, x )           \
        do {                                  \
                if ( !( x ) )                 \
                        asrt_fail( ( rec ) ); \
        } while ( 0 )

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
