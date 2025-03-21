
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
#ifndef ASRTR_RECORD_H
#define ASRTR_RECORD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "./status.h"

#include <stdint.h>

enum asrtr_test_state
{
        ASRTR_TEST_INIT    = 1,  // Test was not yet run
        ASRTR_TEST_RUNNING = 2,  // Test is still running
        ASRTR_TEST_ERROR   = 3,  // Error in test suite
        ASRTR_TEST_FAIL    = 4,  // Test did not pass
        ASRTR_TEST_PASS    = 5,  // Test succeeded
};

struct asrtr_record;
typedef enum asrtr_status ( *asrtr_test_callback )( struct asrtr_record* );

struct asrtr_record
{
        enum asrtr_test_state state;
        void*                 test_ptr;
        asrtr_test_callback   continue_f;
        uint32_t              run_id;
        uint32_t              line;
};

uint32_t asrtr_assert( struct asrtr_record* rec, uint32_t x, uint32_t line );

#define ASRTR_CHECK( rec, x )                             \
        do {                                              \
                asrtr_assert( ( rec ), ( x ), __LINE__ ); \
        } while ( 0 )

#define ASRTR_REQUIRE( rec, x )                                      \
        do {                                                         \
                if ( asrtr_assert( ( rec ), ( x ), __LINE__ ) == 0 ) \
                        return ASRTR_SUCCESS;                        \
        } while ( 0 )

#ifdef __cplusplus
}
#endif

#endif
