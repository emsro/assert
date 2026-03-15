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

#include "../asrtl/core_proto.h"
#include "../asrtl/ecode.h"
#include "../asrtl/log.h"
#include "../asrtl/proto_version.h"
#include "../asrtr/diag.h"
#include "../asrtr/reactor.h"
#include "./asrtr_tests.h"
#include "./collector.h"
#include "./util.h"

#include <stdarg.h>
#include <time.h>
#include <unity.h>

ASRTL_DEFINE_GPOS_LOG()

void setUp( void )
{
}
void tearDown( void )
{
}

//---------------------------------------------------------------------
// lib

void setup_test(
    struct asrtr_reactor* r,
    struct asrtr_test*    t,
    char const*           name,
    void*                 data,
    asrtr_test_callback   start_f )
{
        assert( r );
        assert( t );
        enum asrtr_status st = asrtr_test_init( t, name, data, start_f );
        TEST_ASSERT_EQUAL( ASRTR_SUCCESS, st );
        st = asrtr_reactor_add_test( r, t );
        TEST_ASSERT_EQUAL( ASRTR_SUCCESS, st );
}

void check_reactor_init( struct asrtr_reactor* reac, struct asrtl_sender sender, char const* desc )
{
        enum asrtr_status st = asrtr_reactor_init( reac, sender, desc );
        TEST_ASSERT_EQUAL( ASRTR_SUCCESS, st );
}

void check_diag_init( struct asrtr_diag* diag, struct asrtl_node* prev, struct asrtl_sender sender )
{
        enum asrtr_status st = asrtr_diag_init( diag, prev, sender );
        TEST_ASSERT_EQUAL( ASRTR_SUCCESS, st );
}

void check_reactor_recv( struct asrtr_reactor* reac, struct asrtl_span msg )
{
        enum asrtl_status st = asrtr_reactor_recv( reac, msg );
        TEST_ASSERT_EQUAL( ASRTL_SUCCESS, st );
}

void check_reactor_recv_flags( struct asrtr_reactor* reac, struct asrtl_span msg, uint32_t flags )
{
        check_reactor_recv( reac, msg );
        TEST_ASSERT_EQUAL( flags, reac->flags & ~ASRTR_PASSIVE_FLAGS );
}

void check_reactor_tick( struct asrtr_reactor* reac )
{
        enum asrtr_status st = asrtr_reactor_tick( reac );
        TEST_ASSERT_EQUAL( ASRTR_SUCCESS, st );
        TEST_ASSERT_EQUAL( 0x00, reac->flags & ~ASRTR_PASSIVE_FLAGS );
}

void check_recv_and_spin(
    struct asrtr_reactor*    reac,
    uint8_t*                 beg,
    uint8_t*                 end,
    enum asrtr_reactor_flags fls )
{
        check_reactor_recv_flags( reac, ( struct asrtl_span ){ beg, end }, fls );
        int       i = 0;
        int const n = 1000;
        for ( ; i < n; i++ ) {
                check_reactor_tick( reac );
                if ( reac->state == ASRTR_REAC_IDLE )
                        break;
        }
        TEST_ASSERT_NOT_EQUAL( i, n );
}

void check_run_test( struct asrtr_reactor* reac, uint32_t test_id, uint32_t run_id )
{
        uint8_t           buffer[64];
        struct asrtl_span sp = { .b = buffer, .e = buffer + sizeof buffer };
        enum asrtl_status st = asrtl_msg_ctor_test_start( &sp, test_id, run_id );
        TEST_ASSERT_EQUAL( ASRTL_SUCCESS, st );
        check_recv_and_spin( reac, buffer, sp.b, ASRTR_FLAG_TSTART );
}

void assert_test_result( struct data_ll* collected, uint32_t id, enum asrtl_test_result_e result )
{
        assert_collected_hdr( collected, 0x08, ASRTL_MSG_TEST_RESULT );
        assert_u32( id, collected->data + 2 );
        assert_u16( result, collected->data + 6 );
}

void assert_test_start( struct data_ll* collected, uint16_t test_id, uint32_t run_id )
{
        assert_collected_hdr( collected, 0x08, ASRTL_MSG_TEST_START );
        assert_u16( test_id, collected->data + 2 );
        assert_u32( run_id, collected->data + 4 );
}

struct test_context
{
        struct asrtr_reactor reac;
        struct data_ll*      collected;
        struct asrtl_sender  send;
        // XXX: are these necessary anymore?
        uint8_t           buffer[64];
        struct asrtl_span sp;
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
                    .collected = NULL,
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

enum asrtr_status dataless_test_fun( struct asrtr_record* x )
{
        (void) x;
        return ASRTR_SUCCESS;
}

void test_reactor_init( struct test_context* ctx )
{
        enum asrtr_status st;

        st = asrtr_reactor_init( NULL, ctx->send, "rec1" );
        TEST_ASSERT_EQUAL( ASRTR_INIT_ERR, st );

        st = asrtr_reactor_init( &ctx->reac, ctx->send, NULL );
        TEST_ASSERT_EQUAL( ASRTR_INIT_ERR, st );

        st = asrtr_reactor_init( &ctx->reac, ctx->send, "rec1" );
        TEST_ASSERT_NULL( ctx->reac.first_test );
        TEST_ASSERT_EQUAL( ctx->reac.node.chid, ASRTL_CORE );
        TEST_ASSERT_EQUAL( ctx->reac.state, ASRTR_REAC_IDLE );

        struct asrtr_test t1, t2;
        st = asrtr_test_init( &t1, NULL, NULL, &dataless_test_fun );
        TEST_ASSERT_EQUAL( st, ASRTR_TEST_INIT_ERR );
        st = asrtr_test_init( &t1, "test1", NULL, NULL );
        TEST_ASSERT_EQUAL( st, ASRTR_TEST_INIT_ERR );

        setup_test( &ctx->reac, &t1, "test1", NULL, &dataless_test_fun );
        setup_test( &ctx->reac, &t2, "test2", NULL, &dataless_test_fun );

        TEST_ASSERT_NULL( t2.next );
        TEST_ASSERT_EQUAL_PTR( t1.next, &t2 );
        TEST_ASSERT_EQUAL_PTR( ctx->reac.first_test, &t1 );
}

void test_reactor_version( struct test_context* ctx )
{
        check_reactor_init( &ctx->reac, ctx->send, "rec1" );

        asrtl_msg_ctor_proto_version( &ctx->sp );

        check_recv_and_spin( &ctx->reac, ctx->buffer, ctx->sp.b, ASRTR_FLAG_PROTO_VER );

        assert_collected_hdr( ctx->collected, 0x08, ASRTL_MSG_PROTO_VERSION );
        assert_u16( ASRTL_PROTO_MAJOR, ctx->collected->data + 2 );
        assert_u16( ASRTL_PROTO_MINOR, ctx->collected->data + 4 );
        assert_u16( ASRTL_PROTO_PATCH, ctx->collected->data + 6 );

        clear_single_collected( &ctx->collected );
}

void test_reactor_desc( struct test_context* ctx )
{
        check_reactor_init( &ctx->reac, ctx->send, "rec1" );

        asrtl_msg_ctor_desc( &ctx->sp );

        check_recv_and_spin( &ctx->reac, ctx->buffer, ctx->sp.b, ASRTR_FLAG_DESC );

        assert_collected_hdr( ctx->collected, 0x06, ASRTL_MSG_DESC );
        assert_data_ll_contain_str( "rec1", ctx->collected, 2 );

        clear_single_collected( &ctx->collected );
}

void test_reactor_test_count( struct test_context* ctx )
{
        check_reactor_init( &ctx->reac, ctx->send, "rec1" );

        asrtl_msg_ctor_test_count( &ctx->sp );

        check_recv_and_spin( &ctx->reac, ctx->buffer, ctx->sp.b, ASRTR_FLAG_TC );

        assert_collected_hdr( ctx->collected, 0x04, ASRTL_MSG_TEST_COUNT );
        assert_u16( 0x00, ctx->collected->data + 2 );
        clear_single_collected( &ctx->collected );

        // re-init to add a test before any recv
        check_reactor_init( &ctx->reac, ctx->send, "rec1" );
        struct asrtr_test t1;
        setup_test( &ctx->reac, &t1, "test1", NULL, &dataless_test_fun );

        check_recv_and_spin( &ctx->reac, ctx->buffer, ctx->sp.b, ASRTR_FLAG_TC );

        assert_collected_hdr( ctx->collected, 0x04, ASRTL_MSG_TEST_COUNT );
        assert_u16( 0x01, ctx->collected->data + 2 );
        clear_single_collected( &ctx->collected );
}

void test_reactor_test_info( struct test_context* ctx )
{
        check_reactor_init( &ctx->reac, ctx->send, "rec1" );

        asrtl_msg_ctor_test_info( &ctx->sp, 0 );

        check_recv_and_spin( &ctx->reac, ctx->buffer, ctx->sp.b, ASRTR_FLAG_TI );

        assert_collected_hdr( ctx->collected, 0x04, ASRTL_MSG_ERROR );
        assert_u16( ASRTL_ASE_MISSING_TEST, &ctx->collected->data[2] );
        clear_single_collected( &ctx->collected );

        // re-init to add a test before any recv
        check_reactor_init( &ctx->reac, ctx->send, "rec1" );
        struct asrtr_test t1;
        setup_test( &ctx->reac, &t1, "test1", NULL, &dataless_test_fun );

        check_recv_and_spin( &ctx->reac, ctx->buffer, ctx->sp.b, ASRTR_FLAG_TI );

        assert_collected_hdr( ctx->collected, 0x09, ASRTL_MSG_TEST_INFO );
        assert_u16( 0x00, ctx->collected->data + 2 );
        assert_data_ll_contain_str( "test1", ctx->collected, 4 );
        clear_single_collected( &ctx->collected );
}

void test_reactor_start( struct test_context* ctx )
{
        check_reactor_init( &ctx->reac, ctx->send, "rec1" );

        struct asrtr_test      t1;
        struct insta_test_data data = { .state = ASRTR_TEST_PASS, .counter = 0 };
        setup_test( &ctx->reac, &t1, "test1", &data, &insta_test_fun );

        // just run one test
        check_run_test( &ctx->reac, 0, 0 );

        TEST_ASSERT_EQUAL( 1, data.counter );
        assert_test_result( ctx->collected, 0, ASRTL_TEST_SUCCESS );
        clear_top_collected( &ctx->collected );

        assert_test_start( ctx->collected, 0, 0 );
        clear_single_collected( &ctx->collected );

        asrtl_msg_ctor_test_start( &ctx->sp, 42, 0 );
        check_recv_and_spin( &ctx->reac, ctx->buffer, ctx->sp.b, ASRTR_FLAG_TSTART );

        TEST_ASSERT_EQUAL( 1, data.counter );
        assert_collected_hdr( ctx->collected, 4, ASRTL_MSG_ERROR );
        assert_u16( ASRTL_ASE_MISSING_TEST, &ctx->collected->data[2] );
        clear_single_collected( &ctx->collected );
}

void test_reactor_start_busy( struct test_context* ctx )
{
        check_reactor_init( &ctx->reac, ctx->send, "rec1" );

        struct asrtr_test t1;
        uint64_t          counter = 8;
        setup_test( &ctx->reac, &t1, "test1", &counter, &countdown_test );

        asrtl_msg_ctor_test_start( &ctx->sp, 0, 0 );
        check_reactor_recv_flags(
            &ctx->reac, ( struct asrtl_span ){ ctx->buffer, ctx->sp.b }, ASRTR_FLAG_TSTART );

        check_reactor_tick( &ctx->reac );
        TEST_ASSERT_EQUAL( 8, counter );
        check_reactor_tick( &ctx->reac );
        TEST_ASSERT_EQUAL( 7, counter );

        assert_test_start( ctx->collected, 0, 0 );
        clear_single_collected( &ctx->collected );

        check_reactor_recv_flags(
            &ctx->reac, ( struct asrtl_span ){ ctx->buffer, ctx->sp.b }, ASRTR_FLAG_TSTART );

        check_reactor_tick( &ctx->reac );
        TEST_ASSERT_EQUAL( 7, counter );

        assert_collected_hdr( ctx->collected, 0x04, ASRTL_MSG_ERROR );
        assert_u16( ASRTL_ASE_TEST_ALREADY_RUNNING, &ctx->collected->data[2] );
        clear_single_collected( &ctx->collected );
}

void test_check_macro( struct test_context* ctx )
{
        check_reactor_init( &ctx->reac, ctx->send, "rec1" );
        struct asrtr_test t1;
        struct asrtr_diag diag;
        check_diag_init( &diag, &ctx->reac.node, ctx->send );
        struct astrt_check_ctx check_ctx = {
            .diag    = &diag,
            .counter = 0,
        };
        setup_test( &ctx->reac, &t1, "test1", &check_ctx, &check_macro_test );

        check_run_test( &ctx->reac, 0, 0 );

        TEST_ASSERT_EQUAL( 2, check_ctx.counter );
        assert_test_result( ctx->collected, 0, ASRTL_TEST_FAILURE );
        clear_top_collected( &ctx->collected );

        assert_test_start( ctx->collected, 0, 0 );
        clear_top_collected( &ctx->collected );
}

void test_require_macro( struct test_context* ctx )
{
        check_reactor_init( &ctx->reac, ctx->send, "rec1" );
        struct asrtr_test t1;
        struct asrtr_diag diag;
        check_diag_init( &diag, &ctx->reac.node, ctx->send );
        struct astrt_check_ctx check_ctx = {
            .diag    = &diag,
            .counter = 0,
        };
        setup_test( &ctx->reac, &t1, "test1", &check_ctx, &require_macro_test );

        check_run_test( &ctx->reac, 0, 0 );

        TEST_ASSERT_EQUAL( 1, check_ctx.counter );
        assert_test_result( ctx->collected, 0, ASRTL_TEST_FAILURE );
        clear_top_collected( &ctx->collected );

        assert_test_start( ctx->collected, 0, 0 );
        clear_top_collected( &ctx->collected );
}

void test_test_counter( struct test_context* ctx )
{
        check_reactor_init( &ctx->reac, ctx->send, "rec1" );

        struct asrtr_test t1;
        uint64_t          counter = 0;
        setup_test( &ctx->reac, &t1, "test1", &counter, &countdown_test );

        for ( uint32_t x = 0; x < 42; x++ ) {
                counter = 1;
                check_run_test( &ctx->reac, 0, x );
                assert_test_result( ctx->collected, x, ASRTL_TEST_SUCCESS );
                clear_top_collected( &ctx->collected );

                assert_test_start( ctx->collected, 0, x );
                clear_top_collected( &ctx->collected );
        }
}

void test_reactor_unknown_flag( struct test_context* ctx )
{
        TEST_ASSERT_EQUAL( ASRTR_SUCCESS, asrtr_reactor_init( &ctx->reac, ctx->send, "desc" ) );
        // set only unknown flag bits (known flags are 0x01..0x20); the else branch must signal an
        // error
        ctx->reac.flags      = 0x40;
        enum asrtr_status st = asrtr_reactor_tick( &ctx->reac );
        TEST_ASSERT_NOT_EQUAL( ASRTR_SUCCESS, st );
}

// R03: duplicate TEST_INFO before tick must be rejected
void test_reactor_test_info_repeat( struct test_context* ctx )
{
        check_reactor_init( &ctx->reac, ctx->send, "rec1" );

        struct asrtr_test t1;
        setup_test( &ctx->reac, &t1, "test1", NULL, &dataless_test_fun );

        asrtl_msg_ctor_test_info( &ctx->sp, 0 );

        // first recv — flag must be set
        check_reactor_recv_flags(
            &ctx->reac, ( struct asrtl_span ){ ctx->buffer, ctx->sp.b }, ASRTR_FLAG_TI );

        // second recv before tick — must be rejected
        enum asrtl_status st =
            asrtr_reactor_recv( &ctx->reac, ( struct asrtl_span ){ ctx->buffer, ctx->sp.b } );
        TEST_ASSERT_EQUAL( ASRTL_RECV_UNEXPECTED_ERR, st );

        // flag must still be set
        TEST_ASSERT_EQUAL( ASRTR_FLAG_TI, ctx->reac.flags & ~ASRTR_PASSIVE_FLAGS );
}

// R03: duplicate TEST_START before tick must be rejected
void test_reactor_test_start_repeat( struct test_context* ctx )
{
        check_reactor_init( &ctx->reac, ctx->send, "rec1" );

        struct asrtr_test t1;
        setup_test( &ctx->reac, &t1, "test1", NULL, &dataless_test_fun );

        asrtl_msg_ctor_test_start( &ctx->sp, 0, 42 );

        // first recv — flag must be set
        check_reactor_recv_flags(
            &ctx->reac, ( struct asrtl_span ){ ctx->buffer, ctx->sp.b }, ASRTR_FLAG_TSTART );

        // second recv before tick — must be rejected
        enum asrtl_status st =
            asrtr_reactor_recv( &ctx->reac, ( struct asrtl_span ){ ctx->buffer, ctx->sp.b } );
        TEST_ASSERT_EQUAL( ASRTL_RECV_UNEXPECTED_ERR, st );

        // flag must still be set
        TEST_ASSERT_EQUAL( ASRTR_FLAG_TSTART, ctx->reac.flags & ~ASRTR_PASSIVE_FLAGS );
}

// R04: add_test must be rejected after the first recv call
void test_reactor_add_test_after_recv( struct test_context* ctx )
{
        check_reactor_init( &ctx->reac, ctx->send, "rec1" );

        struct asrtr_test t1, t2;
        setup_test( &ctx->reac, &t1, "test1", NULL, &dataless_test_fun );

        // any valid recv locks registration
        asrtl_msg_ctor_proto_version( &ctx->sp );
        check_reactor_recv( &ctx->reac, ( struct asrtl_span ){ ctx->buffer, ctx->sp.b } );

        // adding a test after recv must be rejected
        enum asrtr_status st = asrtr_test_init( &t2, "test2", NULL, &dataless_test_fun );
        TEST_ASSERT_EQUAL( ASRTR_SUCCESS, st );
        st = asrtr_reactor_add_test( &ctx->reac, &t2 );
        TEST_ASSERT_EQUAL( ASRTR_TEST_REG_ERR, st );

        // test list must not have grown
        TEST_ASSERT_NULL( t1.next );
}

// R-cov1: continue_f returning an error sets ASRTR_TEST_ERROR → sends ASRTL_TEST_ERROR result
void test_reactor_test_error( struct test_context* ctx )
{
        check_reactor_init( &ctx->reac, ctx->send, "rec1" );

        struct asrtr_test t1;
        setup_test( &ctx->reac, &t1, "test1", NULL, &error_continue_fun );

        check_run_test( &ctx->reac, 0, 0 );

        assert_test_result( ctx->collected, 0, ASRTL_TEST_ERROR );
        clear_top_collected( &ctx->collected );

        assert_test_start( ctx->collected, 0, 0 );
        clear_top_collected( &ctx->collected );
}

// R-cov2: two requests received before any tick — both flags set simultaneously
void test_reactor_multi_flag( struct test_context* ctx )
{
        check_reactor_init( &ctx->reac, ctx->send, "rec1" );

        // First request: test count
        asrtl_msg_ctor_test_count( &ctx->sp );
        uint8_t* end1 = ctx->sp.b;
        check_reactor_recv( &ctx->reac, ( struct asrtl_span ){ ctx->buffer, end1 } );

        // Second request: description (no tick between)
        ctx->sp.b = ctx->buffer;
        asrtl_msg_ctor_desc( &ctx->sp );
        uint8_t* end2 = ctx->sp.b;
        check_reactor_recv( &ctx->reac, ( struct asrtl_span ){ ctx->buffer, end2 } );

        // Both flags must be set at the same time
        TEST_ASSERT( ctx->reac.flags & ASRTR_FLAG_TC );
        TEST_ASSERT( ctx->reac.flags & ASRTR_FLAG_DESC );

        // First tick: DESC handled (highest priority in if-else chain)
        enum asrtr_status st = asrtr_reactor_tick( &ctx->reac );
        TEST_ASSERT_EQUAL( ASRTR_SUCCESS, st );
        assert_collected_hdr( ctx->collected, 0x06, ASRTL_MSG_DESC );
        assert_data_ll_contain_str( "rec1", ctx->collected, 2 );
        clear_single_collected( &ctx->collected );
        TEST_ASSERT( ctx->reac.flags & ASRTR_FLAG_TC );
        TEST_ASSERT( !( ctx->reac.flags & ASRTR_FLAG_DESC ) );

        // Second tick: TC handled
        st = asrtr_reactor_tick( &ctx->reac );
        TEST_ASSERT_EQUAL( ASRTR_SUCCESS, st );
        assert_collected_hdr( ctx->collected, 0x04, ASRTL_MSG_TEST_COUNT );
        assert_u16( 0x00, ctx->collected->data + 2 );
        clear_single_collected( &ctx->collected );
        TEST_ASSERT( !( ctx->reac.flags & ASRTR_FLAG_TC ) );
}

// R-cov3: truncated and trailing-byte recv errors in asrtr_reactor_recv
void test_reactor_recv_truncated( struct test_context* ctx )
{
        check_reactor_init( &ctx->reac, ctx->send, "rec1" );

        uint8_t           buf[16];
        struct asrtl_span sp;
        enum asrtl_status rst;

        // Truncated TEST_INFO: only message ID, no u16 tid
        sp = ( struct asrtl_span ){ .b = buf, .e = buf + sizeof buf };
        asrtl_add_u16( &sp.b, ASRTL_MSG_TEST_INFO );
        rst = asrtr_reactor_recv( &ctx->reac, ( struct asrtl_span ){ .b = buf, .e = sp.b } );
        TEST_ASSERT_EQUAL( ASRTL_RECV_ERR, rst );

        // Truncated TEST_START: only ID + partial tid(2), missing run_id(4)
        sp = ( struct asrtl_span ){ .b = buf, .e = buf + sizeof buf };
        asrtl_add_u16( &sp.b, ASRTL_MSG_TEST_START );
        asrtl_add_u16( &sp.b, 0 );  // tid only, no run_id
        rst = asrtr_reactor_recv( &ctx->reac, ( struct asrtl_span ){ .b = buf, .e = sp.b } );
        TEST_ASSERT_EQUAL( ASRTL_RECV_ERR, rst );

        // Trailing bytes: PROTO_VERSION request + extra bytes
        sp = ( struct asrtl_span ){ .b = buf, .e = buf + sizeof buf };
        asrtl_add_u16( &sp.b, ASRTL_MSG_PROTO_VERSION );
        asrtl_add_u16( &sp.b, 0xFFFF );  // extra bytes after a no-payload message
        rst = asrtr_reactor_recv( &ctx->reac, ( struct asrtl_span ){ .b = buf, .e = sp.b } );
        TEST_ASSERT_EQUAL( ASRTL_RECV_ERR, rst );
}

int main( void )
{
        UNITY_BEGIN();
        ASRT_RUN_TEST( test_reactor_init );
        ASRT_RUN_TEST( test_reactor_version );
        ASRT_RUN_TEST( test_reactor_desc );
        ASRT_RUN_TEST( test_reactor_test_count );
        ASRT_RUN_TEST( test_reactor_test_info );
        ASRT_RUN_TEST( test_reactor_start );
        ASRT_RUN_TEST( test_reactor_start_busy );
        ASRT_RUN_TEST( test_check_macro );
        ASRT_RUN_TEST( test_require_macro );
        ASRT_RUN_TEST( test_test_counter );
        ASRT_RUN_TEST( test_reactor_unknown_flag );
        ASRT_RUN_TEST( test_reactor_test_info_repeat );
        ASRT_RUN_TEST( test_reactor_test_start_repeat );
        ASRT_RUN_TEST( test_reactor_add_test_after_recv );
        ASRT_RUN_TEST( test_reactor_test_error );
        ASRT_RUN_TEST( test_reactor_multi_flag );
        ASRT_RUN_TEST( test_reactor_recv_truncated );
        return UNITY_END();
}
