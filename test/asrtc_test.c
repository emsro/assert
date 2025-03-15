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

void check_cntr_recv( struct asrtc_controller* c, struct asrtl_span msg )
{
        enum asrtl_status st = asrtc_cntr_recv( c, msg );
        TEST_ASSERT_EQUAL( ASRTL_SUCCESS, st );
}

void check_cntr_tick( struct asrtc_controller* c )
{
        enum asrtc_status st = asrtc_cntr_tick( c );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, st );
}

void check_recv_and_spin( struct asrtc_controller* c, uint8_t* beg, uint8_t* end )
{
        check_cntr_recv( c, ( struct asrtl_span ){ .b = beg, .e = end } );
        int       i = 0;
        int const n = 1000;
        for ( ; i < n && !asrtc_cntr_idle( c ); i++ )
                check_cntr_tick( c );
        TEST_ASSERT_NOT_EQUAL( i, n );
}

struct test_context
{
        struct asrtc_controller cntr;
        struct data_ll*         collected;
        struct asrtl_sender     send;
        uint8_t                 buffer[64];
        struct asrtl_span       sp;
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
                struct test_context ctx = {
                    .cntr      = {},
                    .collected = NULL,
                    .send      = {},
                    .buffer    = {},
                    .sp        = {},
                };
                ctx.sp = ( struct asrtl_span ){
                    .b = ctx.buffer,
                    .e = ctx.buffer + sizeof ctx.buffer,
                };
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

        TEST_ASSERT( !asrtc_cntr_idle( &ctx->cntr ) );

        st = asrtc_cntr_tick( &ctx->cntr );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, st );

        assert_collected_hdr( ctx->collected, 0x02, ASRTL_MSG_PROTO_VERSION );
        clear_single_collected( &ctx->collected );

        asrtl_msg_rtoc_proto_version( &ctx->sp, 0, 1, 0 );
        check_recv_and_spin( &ctx->cntr, ctx->buffer, ctx->sp.b );

        TEST_ASSERT( asrtc_cntr_idle( &ctx->cntr ) );
}

int main( void )
{
        UNITY_BEGIN();
        ASRT_RUN_TEST( test_cntr_init );
        return UNITY_END();
}
