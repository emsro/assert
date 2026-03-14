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

#include "../asrtc/allocator.h"
#include "../asrtc/controller.h"
#include "../asrtc/default_allocator.h"
#include "../asrtc/default_error_cb.h"
#include "../asrtl/core_proto.h"
#include "../asrtl/log.h"
#include "../asrtl/proto_version.h"
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
        uint8_t                 buffer[128];
        struct asrtl_span       sp;
        enum asrtc_status       init_status;
};

enum asrtc_status record_init_cb( void* ptr, enum asrtc_status s )
{
        enum asrtc_status* p = (enum asrtc_status*) ptr;
        *p                   = s;
        return s;
}

void check_cntr_full_init( struct test_context* ctx )
{
        enum asrtc_status st = asrtc_cntr_init(
            &ctx->cntr,
            ctx->send,
            asrtc_default_allocator(),
            asrtc_default_error_cb(),
            &record_init_cb,
            &ctx->init_status,
            0 );
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
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, ctx->init_status );
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

void test_cntr_init( struct test_context* ctx )
{
        enum asrtc_status st;
        st = asrtc_cntr_init(
            NULL,
            ctx->send,
            asrtc_default_allocator(),
            asrtc_default_error_cb(),
            &record_init_cb,
            NULL,
            0 );
        TEST_ASSERT_EQUAL( ASRTC_CNTR_INIT_ERR, st );

        st = asrtc_cntr_init(
            &ctx->cntr,
            ctx->send,
            asrtc_default_allocator(),
            asrtc_default_error_cb(),
            &record_init_cb,
            &ctx->init_status,
            0 );
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
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, ctx->init_status );
}

enum asrtc_status cpy_desc_cb( void* ptr, enum asrtc_status s, char* desc )
{
        (void) s;
        char**   p = (char**) ptr;
        uint32_t n = strlen( desc ) + 1;
        *p         = malloc( n );
        strncpy( *p, desc, n );
        return ASRTC_SUCCESS;
}

void test_cntr_desc( struct test_context* ctx )
{
        enum asrtc_status st;
        check_cntr_full_init( ctx );

        char* p = NULL;
        st      = asrtc_cntr_desc( &ctx->cntr, &cpy_desc_cb, (void*) &p, 0 );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, st );
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

enum asrtc_status cpy_u32_cb( void* ptr, enum asrtc_status s, uint16_t x )
{
        (void) s;
        uint32_t* p = (uint32_t*) ptr;
        *p          = x;
        return ASRTC_SUCCESS;
}

void test_cntr_test_count( struct test_context* ctx )
{
        enum asrtc_status st;
        check_cntr_full_init( ctx );

        uint32_t p = 0;
        st         = asrtc_cntr_test_count( &ctx->cntr, &cpy_u32_cb, (void*) &p, 0 );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, st );
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
        st      = asrtc_cntr_test_info( &ctx->cntr, 42, &cpy_desc_cb, (void*) &p, 0 );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, st );
        check_cntr_tick( &ctx->cntr );

        assert_collected_hdr( ctx->collected, 0x04, ASRTL_MSG_TEST_INFO );
        assert_u16( 42, ctx->collected->data + 2 );
        clear_single_collected( &ctx->collected );

        char* desc = "barbaz";
        asrtl_msg_rtoc_test_info( &ctx->sp, 42, desc, strlen( desc ) );
        check_recv_and_spin( &ctx->cntr, ctx->buffer, ctx->sp.b );

        TEST_ASSERT_NOT_NULL( p );
        TEST_ASSERT_EQUAL_STRING( desc, p );
        // XXX: maybe the callback should get the test id?  // T03
        if ( p != NULL )
                free( p );
}

void test_cntr_test_info_tid_mismatch( struct test_context* ctx )
{
        check_cntr_full_init( ctx );

        char*             p  = NULL;
        enum asrtc_status st = asrtc_cntr_test_info( &ctx->cntr, 42, &cpy_desc_cb, (void*) &p, 0 );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, st );
        check_cntr_tick( &ctx->cntr );
        clear_single_collected( &ctx->collected );

        // Reply with a different tid (99 instead of 42)
        uint8_t           buf[64];
        struct asrtl_span sp   = { .b = buf, .e = buf + sizeof buf };
        char const*       desc = "barbaz";
        asrtl_msg_rtoc_test_info( &sp, 99, desc, strlen( desc ) );
        enum asrtl_status rst =
            asrtc_cntr_recv( &ctx->cntr, ( struct asrtl_span ){ .b = buf, .e = sp.b } );
        TEST_ASSERT_EQUAL( ASRTL_RECV_UNEXPECTED_ERR, rst );

        TEST_ASSERT_NULL( p );
}

enum asrtc_status result_cb( void* ptr, enum asrtc_status s, struct asrtc_result* res )
{
        (void) s;
        struct asrtc_result* r1 = (struct asrtc_result*) ptr;
        *r1                     = *res;
        return ASRTC_SUCCESS;
}

static void* failing_alloc( void* ptr, uint32_t size )
{
        (void) ptr;
        (void) size;
        return NULL;
}
static void failing_free( void* ptr, void* mem )
{
        (void) ptr;
        (void) mem;
}
static struct asrtc_allocator failing_allocator( void )
{
        return ( struct asrtc_allocator ){
            .ptr   = NULL,
            .alloc = &failing_alloc,
            .free  = &failing_free,
        };
}

void test_cntr_desc_alloc_failure( struct test_context* ctx )
{
        enum asrtc_status st = asrtc_cntr_init(
            &ctx->cntr,
            ctx->send,
            failing_allocator(),
            asrtc_default_error_cb(),
            &record_init_cb,
            &ctx->init_status,
            0 );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, st );
        check_cntr_tick( &ctx->cntr );
        clear_single_collected( &ctx->collected );  // discard PROTO_VERSION request

        // Simulate receiving a proto-version reply to advance to IDLE
        uint8_t           buf[64];
        struct asrtl_span sp = { .b = buf, .e = buf + sizeof buf };
        asrtl_msg_rtoc_proto_version( &sp, 0, 1, 0 );
        check_recv_and_spin( &ctx->cntr, buf, sp.b );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, ctx->init_status );

        st = asrtc_cntr_desc( &ctx->cntr, &cpy_desc_cb, NULL, 0 );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, st );
        check_cntr_tick( &ctx->cntr );
        clear_single_collected( &ctx->collected );  // discard DESC request

        // Send a DESC reply — alloc will return NULL, recv must return an error
        sp              = ( struct asrtl_span ){ .b = buf, .e = buf + sizeof buf };
        char const* msg = "hello";
        asrtl_msg_rtoc_desc( &sp, msg, strlen( msg ) );
        enum asrtl_status rst =
            asrtc_cntr_recv( &ctx->cntr, ( struct asrtl_span ){ .b = buf, .e = sp.b } );
        TEST_ASSERT_NOT_EQUAL( ASRTL_SUCCESS, rst );
}

void test_cntr_run_test( struct test_context* ctx )
{
        enum asrtc_status st;
        check_cntr_full_init( ctx );

        struct asrtc_result res;
        st = asrtc_cntr_test_exec( &ctx->cntr, 42, result_cb, &res, 0 );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, st );
        check_cntr_tick( &ctx->cntr );

        assert_collected_hdr( ctx->collected, 0x08, ASRTL_MSG_TEST_START );
        assert_u16( 42, ctx->collected->data + 2 );
        assert_u32( 0, ctx->collected->data + 4 );
        clear_single_collected( &ctx->collected );

        for ( int i = 0; i < 4; i++ )
                check_cntr_tick( &ctx->cntr );

        asrtl_msg_rtoc_test_start( &ctx->sp, 42, 0 );
        check_cntr_recv( &ctx->cntr, ( struct asrtl_span ){ .b = ctx->buffer, .e = ctx->sp.b } );
        for ( int i = 0; i < 4; i++ )
                check_cntr_tick( &ctx->cntr );

        TEST_ASSERT_NULL( ctx->collected );

        uint8_t* b = ctx->sp.b;
        asrtl_msg_rtoc_test_result( &ctx->sp, 0, ASRTL_TEST_SUCCESS );
        check_recv_and_spin( &ctx->cntr, b, ctx->sp.b );

        TEST_ASSERT_EQUAL( res.test_id, 42 );
        TEST_ASSERT_EQUAL( res.run_id, 0 );
        TEST_ASSERT_EQUAL( res.res, ASRTC_TEST_SUCCESS );
        TEST_ASSERT_NULL( ctx->collected );
}

void test_realloc_str_long_string( struct test_context* ctx )
{
        (void) ctx;
        // Strings longer than 10000 bytes must be handled — the span bound is sufficient
        uint32_t len  = 10001;
        uint8_t* data = malloc( len );
        TEST_ASSERT_NOT_NULL( data );
        memset( data, 'x', len );
        struct asrtl_span      sp    = { .b = data, .e = data + len };
        struct asrtc_allocator alloc = asrtc_default_allocator();
        char*                  res   = asrtc_realloc_str( &alloc, &sp );
        TEST_ASSERT_NOT_NULL( res );
        TEST_ASSERT_EQUAL( 'x', res[0] );
        TEST_ASSERT_EQUAL( '\0', res[len] );
        free( res );
        free( data );
}

void test_cntr_version_mismatch( struct test_context* ctx )
{
        enum asrtc_status st = asrtc_cntr_init(
            &ctx->cntr,
            ctx->send,
            asrtc_default_allocator(),
            asrtc_default_error_cb(),
            &record_init_cb,
            &ctx->init_status,
            0 );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, st );
        check_cntr_tick( &ctx->cntr );
        clear_single_collected( &ctx->collected );

        // reply with a mismatched major version
        uint8_t           buf[64];
        struct asrtl_span sp = { .b = buf, .e = buf + sizeof buf };
        asrtl_msg_rtoc_proto_version( &sp, ASRTL_PROTO_MAJOR + 1, 0, 0 );
        check_cntr_recv( &ctx->cntr, ( struct asrtl_span ){ .b = buf, .e = sp.b } );

        st = asrtc_cntr_tick( &ctx->cntr );
        TEST_ASSERT_EQUAL( ASRTC_VERSION_ERR, st );
        TEST_ASSERT_EQUAL( ASRTC_VERSION_ERR, ctx->init_status );
        TEST_ASSERT( asrtc_cntr_idle( &ctx->cntr ) );
}

// ---------------------------------------------------------------------------
// timeout tests

static enum asrtc_status record_status_cb( void* ptr, enum asrtc_status s )
{
        enum asrtc_status* p = (enum asrtc_status*) ptr;
        *p                   = s;
        return ASRTC_SUCCESS;
}

static enum asrtc_status record_tc_cb( void* ptr, enum asrtc_status s, uint16_t count )
{
        (void) count;
        return record_status_cb( ptr, s );
}

static enum asrtc_status record_desc_cb( void* ptr, enum asrtc_status s, char* desc )
{
        (void) desc;
        return record_status_cb( ptr, s );
}

static enum asrtc_status record_result_cb(
    void*                ptr,
    enum asrtc_status    s,
    struct asrtc_result* res )
{
        (void) res;
        return record_status_cb( ptr, s );
}

void test_cntr_timeout_init( struct test_context* ctx )
{
        enum asrtc_status st = asrtc_cntr_init(
            &ctx->cntr,
            ctx->send,
            asrtc_default_allocator(),
            asrtc_default_error_cb(),
            &record_init_cb,
            &ctx->init_status,
            3 );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, st );

        // first tick sends the request and transitions to WAITING
        check_cntr_tick( &ctx->cntr );
        clear_single_collected( &ctx->collected );

        // 2 waiting ticks — not yet timed out
        check_cntr_tick( &ctx->cntr );
        check_cntr_tick( &ctx->cntr );
        TEST_ASSERT( !asrtc_cntr_idle( &ctx->cntr ) );

        // 3rd waiting tick triggers timeout
        st = asrtc_cntr_tick( &ctx->cntr );
        TEST_ASSERT_EQUAL( ASRTC_TIMEOUT_ERR, st );
        TEST_ASSERT_EQUAL( ASRTC_TIMEOUT_ERR, ctx->init_status );
        TEST_ASSERT( asrtc_cntr_idle( &ctx->cntr ) );
}

void test_cntr_timeout_test_count( struct test_context* ctx )
{
        check_cntr_full_init( ctx );

        enum asrtc_status cb_status = ASRTC_SUCCESS;
        enum asrtc_status st = asrtc_cntr_test_count( &ctx->cntr, &record_tc_cb, &cb_status, 3 );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, st );

        check_cntr_tick( &ctx->cntr );
        clear_single_collected( &ctx->collected );

        check_cntr_tick( &ctx->cntr );
        check_cntr_tick( &ctx->cntr );
        TEST_ASSERT( !asrtc_cntr_idle( &ctx->cntr ) );

        st = asrtc_cntr_tick( &ctx->cntr );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, st );
        TEST_ASSERT_EQUAL( ASRTC_TIMEOUT_ERR, cb_status );
        TEST_ASSERT( asrtc_cntr_idle( &ctx->cntr ) );
}

void test_cntr_timeout_desc( struct test_context* ctx )
{
        check_cntr_full_init( ctx );

        enum asrtc_status cb_status = ASRTC_SUCCESS;
        enum asrtc_status st        = asrtc_cntr_desc( &ctx->cntr, &record_desc_cb, &cb_status, 3 );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, st );

        check_cntr_tick( &ctx->cntr );
        clear_single_collected( &ctx->collected );

        check_cntr_tick( &ctx->cntr );
        check_cntr_tick( &ctx->cntr );
        TEST_ASSERT( !asrtc_cntr_idle( &ctx->cntr ) );

        st = asrtc_cntr_tick( &ctx->cntr );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, st );
        TEST_ASSERT_EQUAL( ASRTC_TIMEOUT_ERR, cb_status );
        TEST_ASSERT( asrtc_cntr_idle( &ctx->cntr ) );
}

void test_cntr_timeout_test_info( struct test_context* ctx )
{
        check_cntr_full_init( ctx );

        enum asrtc_status cb_status = ASRTC_SUCCESS;
        enum asrtc_status st =
            asrtc_cntr_test_info( &ctx->cntr, 0, &record_desc_cb, &cb_status, 3 );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, st );

        check_cntr_tick( &ctx->cntr );
        clear_single_collected( &ctx->collected );

        check_cntr_tick( &ctx->cntr );
        check_cntr_tick( &ctx->cntr );
        TEST_ASSERT( !asrtc_cntr_idle( &ctx->cntr ) );

        st = asrtc_cntr_tick( &ctx->cntr );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, st );
        TEST_ASSERT_EQUAL( ASRTC_TIMEOUT_ERR, cb_status );
        TEST_ASSERT( asrtc_cntr_idle( &ctx->cntr ) );
}

void test_cntr_timeout_exec( struct test_context* ctx )
{
        check_cntr_full_init( ctx );

        enum asrtc_status cb_status = ASRTC_SUCCESS;
        enum asrtc_status st =
            asrtc_cntr_test_exec( &ctx->cntr, 0, &record_result_cb, &cb_status, 3 );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, st );

        check_cntr_tick( &ctx->cntr );
        clear_single_collected( &ctx->collected );

        check_cntr_tick( &ctx->cntr );
        check_cntr_tick( &ctx->cntr );
        TEST_ASSERT( !asrtc_cntr_idle( &ctx->cntr ) );

        st = asrtc_cntr_tick( &ctx->cntr );
        TEST_ASSERT_EQUAL( ASRTC_SUCCESS, st );
        TEST_ASSERT_EQUAL( ASRTC_TIMEOUT_ERR, cb_status );
        TEST_ASSERT( asrtc_cntr_idle( &ctx->cntr ) );
}

int main( void )
{
        UNITY_BEGIN();
        ASRT_RUN_TEST( test_cntr_init );
        ASRT_RUN_TEST( test_cntr_desc );
        ASRT_RUN_TEST( test_cntr_test_count );
        ASRT_RUN_TEST( test_cntr_test_info );
        ASRT_RUN_TEST( test_cntr_test_info_tid_mismatch );
        ASRT_RUN_TEST( test_cntr_run_test );
        ASRT_RUN_TEST( test_cntr_desc_alloc_failure );
        ASRT_RUN_TEST( test_realloc_str_long_string );
        ASRT_RUN_TEST( test_cntr_version_mismatch );
        ASRT_RUN_TEST( test_cntr_timeout_init );
        ASRT_RUN_TEST( test_cntr_timeout_test_count );
        ASRT_RUN_TEST( test_cntr_timeout_desc );
        ASRT_RUN_TEST( test_cntr_timeout_test_info );
        ASRT_RUN_TEST( test_cntr_timeout_exec );
        return UNITY_END();
}
