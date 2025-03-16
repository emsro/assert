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

void check_cntr_full_init( struct test_context* ctx )
{
        enum asrtc_status st = asrtc_cntr_init( &ctx->cntr, ctx->send, asrtc_default_allocator() );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, st );
        check_cntr_tick( &ctx->cntr );

        assert_collected_hdr( ctx->collected, 0x02, ASRTL_MSG_PROTO_VERSION );
        clear_single_collected( &ctx->collected );

        uint8_t           buffer[64];
        struct asrtl_span sp = {
            .b = buffer,
            .e = buffer + sizeof buffer,
        };

        asrtl_msg_rtoc_proto_version( &sp, 0, 1, 0 );
        check_recv_and_spin( &ctx->cntr, buffer, sp.b );
}


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
        st = asrtc_cntr_init( NULL, ctx->send, asrtc_default_allocator() );
        TEST_ASSERT_EQUAL( ASRTC_CNTR_INIT_ERR, st );

        st = asrtc_cntr_init( &ctx->cntr, ctx->send, asrtc_default_allocator() );
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

void cpy_desc_cb( void* ptr, char* desc )
{
        char**   p = (char**) ptr;
        uint32_t n = strlen( desc ) + 1;
        *p         = malloc( n );
        strncpy( *p, desc, n );
}

void test_cntr_desc( struct test_context* ctx )
{
        enum asrtc_status st;
        check_cntr_full_init( ctx );

        char* p = NULL;
        st      = asrtc_cntr_desc( &ctx->cntr, &cpy_desc_cb, (void*) &p );
        check_cntr_tick( &ctx->cntr );

        assert_collected_hdr( ctx->collected, 0x02, ASRTL_MSG_DESC );
        clear_single_collected( &ctx->collected );

        char const* msg = "wololo1";
        asrtl_msg_rtoc_desc( &ctx->sp, msg, strlen( msg ) );
        check_recv_and_spin( &ctx->cntr, ctx->buffer, ctx->sp.b );

        TEST_ASSERT_NOT_NULL( p );
        TEST_ASSERT_EQUAL_STRING( msg, p );

        if ( p != NULL )
                free( p );
}

void cpy_u32_cb( void* ptr, uint16_t x )
{
        uint32_t* p = (uint32_t*) ptr;
        *p          = x;
}

void test_cntr_test_count( struct test_context* ctx )
{
        enum asrtc_status st;
        check_cntr_full_init( ctx );

        uint32_t p = 0;
        st         = asrtc_cntr_test_count( &ctx->cntr, &cpy_u32_cb, (void*) &p );
        check_cntr_tick( &ctx->cntr );

        assert_collected_hdr( ctx->collected, 0x02, ASRTL_MSG_TEST_COUNT );
        clear_single_collected( &ctx->collected );

        asrtl_msg_rtoc_test_count( &ctx->sp, 42 );
        check_recv_and_spin( &ctx->cntr, ctx->buffer, ctx->sp.b );

        TEST_ASSERT_EQUAL( 42, p );
}

void test_cntr_test_info( struct test_context* ctx )
{
        enum asrtc_status st;
        check_cntr_full_init( ctx );

        char* p = NULL;
        st      = asrtc_cntr_test_info( &ctx->cntr, 42, &cpy_desc_cb, (void*) &p );
        check_cntr_tick( &ctx->cntr );

        assert_collected_hdr( ctx->collected, 0x04, ASRTL_MSG_TEST_INFO );
        assert_u16( 42, ctx->collected->data + 2 );
        clear_single_collected( &ctx->collected );

        char* desc = "barbaz";
        asrtl_msg_rtoc_test_info( &ctx->sp, 42, desc, strlen( desc ) );
        check_recv_and_spin( &ctx->cntr, ctx->buffer, ctx->sp.b );

        TEST_ASSERT_NOT_NULL( p );
        TEST_ASSERT_EQUAL_STRING( desc, p );
        // XXX: maybe the callback should get the test id?
        if ( p != NULL )
                free( p );
}

int main( void )
{
        UNITY_BEGIN();
        ASRT_RUN_TEST( test_cntr_init );
        ASRT_RUN_TEST( test_cntr_desc );
        ASRT_RUN_TEST( test_cntr_test_count );
        ASRT_RUN_TEST( test_cntr_test_info );
        return UNITY_END();
}
