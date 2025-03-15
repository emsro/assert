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
#define UNITY_SKIP_DEFAULT_RUNNER

#include "../asrtc/controller.h"
#include "../asrtc/default_allocator.h"
#include "../asrtl/core_proto.h"
#include "./collector.h"
#include "./util.h"

#include <unity.h>

void setUp()
{
}
void tearDown()
{
}

//---------------------------------------------------------------------
// lib

struct test_context
{
        struct asrtc_controller cntr;
        struct data_ll*         collected;
        struct asrtl_sender     send;
};

void test_run(
    void ( *test_fn )( struct test_context* ),
    char const* FuncName,
    int const   FuncLineNum )
{
        Unity.CurrentTestName       = FuncName;
        Unity.CurrentTestLineNumber = (UNITY_LINE_TYPE) FuncLineNum;
        Unity.NumberOfTests++;
        UNITY_CLR_DETAILS();
        UNITY_EXEC_TIME_START();
        if ( TEST_PROTECT() ) {
                struct test_context ctx = { .cntr = {}, .collected = NULL, .send = {} };
                setup_sender_collector( &ctx.send, &ctx.collected );
                test_fn( &ctx );
                TEST_ASSERT_NULL( ctx.collected );
                if ( ctx.collected )
                        rec_free_data_ll( ctx.collected );
        }
        UNITY_EXEC_TIME_STOP();
        UnityConcludeTest();
}

#define ASRT_RUN_TEST( func ) test_run( func, #func, __LINE__ );

//---------------------------------------------------------------------
// tests

void test_cntr_init( struct test_context* ctx )
{
        enum asrtc_status st;
        st = asrtc_cntr_init( NULL, ctx->send, default_allocator() );
        TEST_ASSERT_EQUAL( ASRTC_CNTR_INIT_ERR, st );

        st = asrtc_cntr_init( &ctx->cntr, ctx->send, default_allocator() );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, st );
        TEST_ASSERT_EQUAL( ASRTL_CORE, ctx->cntr.node.chid );
        TEST_ASSERT_EQUAL( ASRTC_CNTR_INIT, ctx->cntr.state );

        TEST_ASSERT( asrtc_cntr_idle( &ctx->cntr ) );
}

int main( void )
{
        UNITY_BEGIN();
        ASRT_RUN_TEST( test_cntr_init );
        return UNITY_END();
}
