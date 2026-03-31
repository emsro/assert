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
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../asrtc/controller.h"
#include "../asrtc/default_allocator.h"
#include "../asrtc/default_error_cb.h"
#include "../asrtc/diag.h"
#include "../asrtc/param.h"
#include "../asrtl/core_proto.h"
#include "../asrtl/log.h"
#include "../asrtl/proto_version.h"
#include "../asrtl/util.h"
#include "../asrtr/param.h"
#include "./collector.hpp"
#include "./stub_allocator.hpp"
#include "./util.h"

#include <doctest/doctest.h>

ASRTL_DEFINE_GPOS_LOG()

//---------------------------------------------------------------------
// lib

void check_cntr_recv( struct asrtc_controller* c, struct asrtl_span msg )
{
        enum asrtl_status st = asrtc_cntr_recv( c, msg );
        CHECK_EQ( ASRTL_SUCCESS, st );
}

void check_cntr_tick( struct asrtc_controller* c, uint32_t now )
{
        enum asrtc_status st = asrtc_cntr_tick( c, now );
        CHECK_EQ( ASRTC_SUCCESS, st );
}

void check_recv_and_spin( struct asrtc_controller* c, uint8_t* beg, uint8_t* end, uint32_t* now )
{
        check_cntr_recv( c, (struct asrtl_span) { .b = beg, .e = end } );
        int       i = 0;
        int const n = 1000;
        for ( ; i < n && !asrtc_cntr_idle( c ); i++ )
                check_cntr_tick( c, ( *now )++ );
        CHECK_NE( i, n );
}

struct controller_ctx
{
        struct asrtc_controller cntr = {};
        collector               coll;
        struct asrtl_sender     send        = {};
        uint8_t                 buffer[128] = {};
        struct asrtl_span       sp          = {};
        enum asrtc_status       init_status = {};
        uint32_t                t           = 1;

        controller_ctx()
        {
                sp = { buffer, buffer + sizeof buffer };
                setup_sender_collector( &send, &coll );
        }
        ~controller_ctx()
        {
                CHECK_EQ( coll.data.size(), 0 );
        }
};

enum asrtc_status record_init_cb( void* ptr, enum asrtc_status s )
{
        enum asrtc_status* p = (enum asrtc_status*) ptr;
        *p                   = s;
        return s;
}

void check_cntr_full_init( controller_ctx* ctx )
{
        enum asrtc_status st = asrtc_cntr_init(
            &ctx->cntr, ctx->send, asrtc_default_allocator(), asrtc_default_error_cb() );
        CHECK_EQ( ASRTC_SUCCESS, st );
        st = asrtc_cntr_start( &ctx->cntr, &record_init_cb, &ctx->init_status, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &ctx->cntr, ctx->t++ );

        assert_collected_core_hdr( ctx->coll.data.back(), 0x02, ASRTL_MSG_PROTO_VERSION );
        ctx->coll.data.pop_back();

        uint8_t           buffer[64];
        struct asrtl_span sp = {
            .b = buffer,
            .e = buffer + sizeof buffer,
        };

        asrtl_msg_rtoc_proto_version( 0, 1, 0, asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &ctx->cntr, buffer, sp.b, &ctx->t );
        CHECK_EQ( ASRTC_SUCCESS, ctx->init_status );
}


//---------------------------------------------------------------------
// tests

TEST_CASE_FIXTURE( controller_ctx, "cntr_init" )
{
        enum asrtc_status st;
        st = asrtc_cntr_init( NULL, send, asrtc_default_allocator(), asrtc_default_error_cb() );
        CHECK_EQ( ASRTC_CNTR_INIT_ERR, st );

        st = asrtc_cntr_init( &cntr, send, asrtc_default_allocator(), asrtc_default_error_cb() );
        CHECK_EQ( ASRTC_SUCCESS, st );
        CHECK_EQ( ASRTL_CORE, cntr.node.chid );
        CHECK( asrtc_cntr_idle( &cntr ) );

        st = asrtc_cntr_start( &cntr, &record_init_cb, &init_status, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        CHECK_EQ( ASRTC_CNTR_INIT, cntr.state );
        CHECK( !asrtc_cntr_idle( &cntr ) );

        st = asrtc_cntr_tick( &cntr, t++ );
        CHECK_EQ( ASRTC_SUCCESS, st );

        assert_collected_core_hdr( coll.data.back(), 0x02, ASRTL_MSG_PROTO_VERSION );
        coll.data.pop_back();

        asrtl_msg_rtoc_proto_version( 0, 1, 0, asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, buffer, sp.b, &t );

        CHECK( asrtc_cntr_idle( &cntr ) );
        CHECK_EQ( ASRTC_SUCCESS, init_status );
}

enum asrtc_status cpy_desc_cb( void* ptr, enum asrtc_status s, char* desc )
{
        (void) s;
        std::string* p = (std::string*) ptr;
        *p             = desc;
        return ASRTC_SUCCESS;
}

struct test_info_result
{
        uint16_t    tid;
        std::string desc;
};

enum asrtc_status cpy_test_info_cb( void* ptr, enum asrtc_status s, uint16_t tid, char* desc )
{
        (void) s;
        struct test_info_result* r = (struct test_info_result*) ptr;
        r->tid                     = tid;
        r->desc                    = desc;
        return ASRTC_SUCCESS;
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_desc" )
{
        enum asrtc_status st;
        check_cntr_full_init( this );

        std::string desc;
        st = asrtc_cntr_desc( &cntr, &cpy_desc_cb, (void*) &desc, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr, t++ );

        assert_collected_core_hdr( coll.data.back(), 0x02, ASRTL_MSG_DESC );
        coll.data.pop_back();

        char const* msg = "wololo1";
        asrtl_msg_rtoc_desc( msg, strlen( msg ), asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, buffer, sp.b, &t );

        CHECK( desc != "" );
        CHECK( msg == desc );
}

enum asrtc_status cpy_u32_cb( void* ptr, enum asrtc_status s, uint16_t x )
{
        (void) s;
        uint32_t* p = (uint32_t*) ptr;
        *p          = x;
        return ASRTC_SUCCESS;
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_test_count" )
{
        enum asrtc_status st;
        check_cntr_full_init( this );

        uint32_t p = 0;
        st         = asrtc_cntr_test_count( &cntr, &cpy_u32_cb, (void*) &p, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr, t++ );

        assert_collected_core_hdr( coll.data.back(), 0x02, ASRTL_MSG_TEST_COUNT );
        coll.data.pop_back();

        asrtl_msg_rtoc_test_count( 42, asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, buffer, sp.b, &t );

        CHECK_EQ( 42, p );
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_test_info" )
{
        enum asrtc_status st;
        check_cntr_full_init( this );

        struct test_info_result p = { 0 };
        st = asrtc_cntr_test_info( &cntr, 42, &cpy_test_info_cb, (void*) &p, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr, t++ );

        assert_collected_core_hdr( coll.data.back(), 0x04, ASRTL_MSG_TEST_INFO );
        assert_u16( 42, coll.data.back().data.data() + 2 );
        coll.data.pop_back();

        char const* desc = "barbaz";
        asrtl_msg_rtoc_test_info( 42, desc, strlen( desc ), asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, buffer, sp.b, &t );

        CHECK( desc == p.desc );
        CHECK_EQ( 42, p.tid );
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_test_info_tid_mismatch" )
{
        check_cntr_full_init( this );

        struct test_info_result p = { 0 };
        enum asrtc_status       st =
            asrtc_cntr_test_info( &cntr, 42, &cpy_test_info_cb, (void*) &p, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr, t++ );
        coll.data.pop_back();

        // Reply with a different tid (99 instead of 42)
        uint8_t           buf[64];
        struct asrtl_span sp   = { .b = buf, .e = buf + sizeof buf };
        char const*       desc = "barbaz";
        asrtl_msg_rtoc_test_info( 99, desc, strlen( desc ), asrtl_rec_span_to_span_cb, &sp );
        enum asrtl_status rst =
            asrtc_cntr_recv( &cntr, (struct asrtl_span) { .b = buf, .e = sp.b } );
        CHECK_EQ( ASRTL_RECV_UNEXPECTED_ERR, rst );

        CHECK( p.desc.empty() );
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
static struct asrtl_allocator failing_allocator( void )
{
        return (struct asrtl_allocator) {
            .ptr   = NULL,
            .alloc = &failing_alloc,
            .free  = &failing_free,
        };
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_desc_alloc_failure" )
{
        enum asrtc_status st =
            asrtc_cntr_init( &cntr, send, failing_allocator(), asrtc_default_error_cb() );
        CHECK_EQ( ASRTC_SUCCESS, st );
        st = asrtc_cntr_start( &cntr, &record_init_cb, &init_status, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr, t++ );
        coll.data.pop_back();  // discard PROTO_VERSION request

        // Simulate receiving a proto-version reply to advance to IDLE
        uint8_t           buf[64];
        struct asrtl_span sp = { .b = buf, .e = buf + sizeof buf };
        asrtl_msg_rtoc_proto_version( 0, 1, 0, asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, buf, sp.b, &t );
        CHECK_EQ( ASRTC_SUCCESS, init_status );

        std::string desc;
        st = asrtc_cntr_desc( &cntr, &cpy_desc_cb, (void*) &desc, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr, t++ );
        coll.data.pop_back();  // discard DESC request

        // Send a DESC reply — alloc will return NULL, recv must return an error
        sp              = (struct asrtl_span) { .b = buf, .e = buf + sizeof buf };
        char const* msg = "hello";
        asrtl_msg_rtoc_desc( msg, strlen( msg ), asrtl_rec_span_to_span_cb, &sp );
        enum asrtl_status rst =
            asrtc_cntr_recv( &cntr, (struct asrtl_span) { .b = buf, .e = sp.b } );
        CHECK_NE( ASRTL_SUCCESS, rst );
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_run_test" )
{
        enum asrtc_status st;
        check_cntr_full_init( this );

        struct asrtc_result res;
        st = asrtc_cntr_test_exec( &cntr, 42, result_cb, &res, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr, t++ );

        assert_collected_core_hdr( coll.data.back(), 0x08, ASRTL_MSG_TEST_START );
        assert_u16( 42, coll.data.back().data.data() + 2 );
        assert_u32( 0, coll.data.back().data.data() + 4 );
        coll.data.pop_back();

        for ( int i = 0; i < 4; i++ )
                check_cntr_tick( &cntr, t++ );

        asrtl_msg_rtoc_test_start( 42, 0, asrtl_rec_span_to_span_cb, &sp );
        check_cntr_recv( &cntr, (struct asrtl_span) { .b = buffer, .e = sp.b } );
        for ( int i = 0; i < 4; i++ )
                check_cntr_tick( &cntr, t++ );

        CHECK_EQ( coll.data.empty(), true );

        uint8_t* b = sp.b;
        asrtl_msg_rtoc_test_result( 0, ASRTL_TEST_SUCCESS, asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, b, sp.b, &t );

        CHECK_EQ( res.test_id, 42 );
        CHECK_EQ( res.run_id, 0 );
        CHECK_EQ( res.res, ASRTC_TEST_SUCCESS );
        CHECK_EQ( coll.data.empty(), true );
}

TEST_CASE_FIXTURE( controller_ctx, "realloc_str_long_string" )
{
        // Strings longer than 10000 bytes must be handled — the span bound is sufficient
        uint32_t len  = 10001;
        uint8_t* data = (uint8_t*) malloc( len );
        CHECK_NE( data, nullptr );
        memset( data, 'x', len );
        struct asrtl_span      sp    = { .b = data, .e = data + len };
        struct asrtl_allocator alloc = asrtc_default_allocator();
        char*                  res   = asrtl_realloc_str( &alloc, &sp );
        CHECK_NE( res, nullptr );
        CHECK_EQ( 'x', res[0] );
        CHECK_EQ( '\0', res[len] );
        free( res );
        free( data );
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_version_mismatch" )
{
        enum asrtc_status st =
            asrtc_cntr_init( &cntr, send, asrtc_default_allocator(), asrtc_default_error_cb() );
        CHECK_EQ( ASRTC_SUCCESS, st );
        st = asrtc_cntr_start( &cntr, &record_init_cb, &init_status, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr, t++ );
        coll.data.pop_back();

        // reply with a mismatched major version
        uint8_t           buf[64];
        struct asrtl_span sp = { .b = buf, .e = buf + sizeof buf };
        asrtl_msg_rtoc_proto_version( ASRTL_PROTO_MAJOR + 1, 0, 0, asrtl_rec_span_to_span_cb, &sp );
        check_cntr_recv( &cntr, (struct asrtl_span) { .b = buf, .e = sp.b } );

        st = asrtc_cntr_tick( &cntr, t++ );
        CHECK_EQ( ASRTC_VERSION_ERR, st );
        CHECK_EQ( ASRTC_VERSION_ERR, init_status );
        CHECK( asrtc_cntr_idle( &cntr ) );
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

static enum asrtc_status record_test_info_cb(
    void*             ptr,
    enum asrtc_status s,
    uint16_t          tid,
    char*             desc )
{
        (void) tid;
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

TEST_CASE_FIXTURE( controller_ctx, "cntr_timeout_init" )
{
        enum asrtc_status st =
            asrtc_cntr_init( &cntr, send, asrtc_default_allocator(), asrtc_default_error_cb() );
        CHECK_EQ( ASRTC_SUCCESS, st );
        st = asrtc_cntr_start( &cntr, &record_init_cb, &init_status, 3 );
        CHECK_EQ( ASRTC_SUCCESS, st );

        // now=0: sends request, enters WAITING, deadline = 0+3 = 3
        CHECK_EQ( ASRTC_SUCCESS, asrtc_cntr_tick( &cntr, 0 ) );
        coll.data.pop_back();

        // now=1,2: still waiting
        CHECK_EQ( ASRTC_SUCCESS, asrtc_cntr_tick( &cntr, 1 ) );
        CHECK_EQ( ASRTC_SUCCESS, asrtc_cntr_tick( &cntr, 2 ) );
        CHECK( !asrtc_cntr_idle( &cntr ) );

        // now=3: deadline reached → timeout
        st = asrtc_cntr_tick( &cntr, 3 );
        CHECK_EQ( ASRTC_TIMEOUT_ERR, st );
        CHECK_EQ( ASRTC_TIMEOUT_ERR, init_status );
        CHECK( asrtc_cntr_idle( &cntr ) );
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_timeout_test_count" )
{
        check_cntr_full_init( this );

        enum asrtc_status cb_status = ASRTC_SUCCESS;
        enum asrtc_status st        = asrtc_cntr_test_count( &cntr, &record_tc_cb, &cb_status, 3 );
        CHECK_EQ( ASRTC_SUCCESS, st );

        CHECK_EQ( ASRTC_SUCCESS, asrtc_cntr_tick( &cntr, 0 ) );
        coll.data.pop_back();

        CHECK_EQ( ASRTC_SUCCESS, asrtc_cntr_tick( &cntr, 1 ) );
        CHECK_EQ( ASRTC_SUCCESS, asrtc_cntr_tick( &cntr, 2 ) );
        CHECK( !asrtc_cntr_idle( &cntr ) );

        st = asrtc_cntr_tick( &cntr, 3 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        CHECK_EQ( ASRTC_TIMEOUT_ERR, cb_status );
        CHECK( asrtc_cntr_idle( &cntr ) );
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_timeout_desc" )
{
        check_cntr_full_init( this );

        enum asrtc_status cb_status = ASRTC_SUCCESS;
        enum asrtc_status st        = asrtc_cntr_desc( &cntr, &record_desc_cb, &cb_status, 3 );
        CHECK_EQ( ASRTC_SUCCESS, st );

        CHECK_EQ( ASRTC_SUCCESS, asrtc_cntr_tick( &cntr, 0 ) );
        coll.data.pop_back();

        CHECK_EQ( ASRTC_SUCCESS, asrtc_cntr_tick( &cntr, 1 ) );
        CHECK_EQ( ASRTC_SUCCESS, asrtc_cntr_tick( &cntr, 2 ) );
        CHECK( !asrtc_cntr_idle( &cntr ) );

        st = asrtc_cntr_tick( &cntr, 3 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        CHECK_EQ( ASRTC_TIMEOUT_ERR, cb_status );
        CHECK( asrtc_cntr_idle( &cntr ) );
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_timeout_test_info" )
{
        check_cntr_full_init( this );

        enum asrtc_status cb_status = ASRTC_SUCCESS;
        enum asrtc_status st =
            asrtc_cntr_test_info( &cntr, 0, &record_test_info_cb, &cb_status, 3 );
        CHECK_EQ( ASRTC_SUCCESS, st );

        CHECK_EQ( ASRTC_SUCCESS, asrtc_cntr_tick( &cntr, 0 ) );
        coll.data.pop_back();

        CHECK_EQ( ASRTC_SUCCESS, asrtc_cntr_tick( &cntr, 1 ) );
        CHECK_EQ( ASRTC_SUCCESS, asrtc_cntr_tick( &cntr, 2 ) );
        CHECK( !asrtc_cntr_idle( &cntr ) );

        st = asrtc_cntr_tick( &cntr, 3 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        CHECK_EQ( ASRTC_TIMEOUT_ERR, cb_status );
        CHECK( asrtc_cntr_idle( &cntr ) );
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_timeout_exec" )
{
        check_cntr_full_init( this );

        enum asrtc_status cb_status = ASRTC_SUCCESS;
        enum asrtc_status st = asrtc_cntr_test_exec( &cntr, 0, &record_result_cb, &cb_status, 3 );
        CHECK_EQ( ASRTC_SUCCESS, st );

        // now=0: STAGE_INIT → sends, enters WAITING, deadline = 0+3 = 3
        CHECK_EQ( ASRTC_SUCCESS, asrtc_cntr_tick( &cntr, 0 ) );
        coll.data.pop_back();

        // now=1,2: still waiting
        CHECK_EQ( ASRTC_SUCCESS, asrtc_cntr_tick( &cntr, 1 ) );
        CHECK_EQ( ASRTC_SUCCESS, asrtc_cntr_tick( &cntr, 2 ) );
        CHECK( !asrtc_cntr_idle( &cntr ) );

        // now=3: deadline reached → timeout
        st = asrtc_cntr_tick( &cntr, 3 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        CHECK_EQ( ASRTC_TIMEOUT_ERR, cb_status );
        CHECK( asrtc_cntr_idle( &cntr ) );
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_busy_err" )
{
        check_cntr_full_init( this );

        // Start an operation to put the controller into a non-idle state
        std::string       desc;
        enum asrtc_status st = asrtc_cntr_desc( &cntr, &cpy_desc_cb, (void*) &desc, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        CHECK( !asrtc_cntr_idle( &cntr ) );

        // Any subsequent operation while busy must return BUSY_ERR
        st = asrtc_cntr_desc( &cntr, &cpy_desc_cb, (void*) &desc, 1000 );
        CHECK_EQ( ASRTC_CNTR_BUSY_ERR, st );

        uint32_t x;
        st = asrtc_cntr_test_count( &cntr, &cpy_u32_cb, (void*) &x, 1000 );
        CHECK_EQ( ASRTC_CNTR_BUSY_ERR, st );

        st = asrtc_cntr_test_info( &cntr, 0, &cpy_test_info_cb, (void*) &desc, 1000 );
        CHECK_EQ( ASRTC_CNTR_BUSY_ERR, st );
}

struct error_result
{
        enum asrtl_source src;
        uint16_t          ecode;
};

static enum asrtc_status record_error_cb( void* ptr, enum asrtl_source src, uint16_t ecode )
{
        struct error_result* r = (struct error_result*) ptr;
        r->src                 = src;
        r->ecode               = ecode;
        return ASRTC_SUCCESS;
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_recv_error" )
{
        // Init with a custom error callback that records what it receives
        struct error_result   err = {};
        struct asrtc_error_cb ecb = { .ptr = &err, .cb = &record_error_cb };

        enum asrtc_status st = asrtc_cntr_init( &cntr, send, asrtc_default_allocator(), ecb );
        CHECK_EQ( ASRTC_SUCCESS, st );
        st = asrtc_cntr_start( &cntr, &record_init_cb, &init_status, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr, t++ );
        coll.data.pop_back();

        // Send an error message while the controller is waiting for a response
        uint8_t           buf[16];
        struct asrtl_span sp = { .b = buf, .e = buf + sizeof buf };
        asrtl_msg_rtoc_error( 42, asrtl_rec_span_to_span_cb, &sp );
        enum asrtl_status rst =
            asrtc_cntr_recv( &cntr, (struct asrtl_span) { .b = buf, .e = sp.b } );
        CHECK_EQ( ASRTL_SUCCESS, rst );
        CHECK_EQ( ASRTL_REACTOR, err.src );
        CHECK_EQ( 42, err.ecode );
}

// wrong run_id in TEST_RESULT → ASRTC_TEST_ERROR in callback
TEST_CASE_FIXTURE( controller_ctx, "cntr_test_exec_wrong_run_id" )
{
        check_cntr_full_init( this );

        struct asrtc_result res = { 0 };
        enum asrtc_status   st  = asrtc_cntr_test_exec( &cntr, 42, result_cb, &res, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr, t++ );  // sends TEST_START request
        coll.data.pop_back();

        // Send TEST_RESULT with wrong run_id (controller expects 0, send 99)
        uint8_t           buf[16];
        struct asrtl_span sp = { .b = buf, .e = buf + sizeof buf };
        asrtl_msg_rtoc_test_result( 99, ASRTL_TEST_SUCCESS, asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, buf, sp.b, &t );

        CHECK_EQ( ASRTC_TEST_ERROR, res.res );
        CHECK_EQ( coll.data.empty(), true );
}

// recv while controller is IDLE → ASRTL_RECV_UNEXPECTED_ERR
TEST_CASE_FIXTURE( controller_ctx, "cntr_recv_idle" )
{
        check_cntr_full_init( this );

        uint8_t           buf[16];
        struct asrtl_span sp = { .b = buf, .e = buf + sizeof buf };
        asrtl_msg_rtoc_proto_version( 0, 1, 0, asrtl_rec_span_to_span_cb, &sp );
        enum asrtl_status rst =
            asrtc_cntr_recv( &cntr, (struct asrtl_span) { .b = buf, .e = sp.b } );
        CHECK_EQ( ASRTL_RECV_UNEXPECTED_ERR, rst );
        CHECK( asrtc_cntr_idle( &cntr ) );
}

// empty buffer — top-level header truncation
TEST_CASE_FIXTURE( controller_ctx, "cntr_recv_truncated_hdr" )
{
        enum asrtc_status st =
            asrtc_cntr_init( &cntr, send, asrtc_default_allocator(), asrtc_default_error_cb() );
        CHECK_EQ( ASRTC_SUCCESS, st );
        st = asrtc_cntr_start( &cntr, &record_init_cb, &init_status, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );

        uint8_t           buf[1];
        enum asrtl_status rst =
            asrtc_cntr_recv( &cntr, (struct asrtl_span) { .b = buf, .e = buf } );
        CHECK_EQ( ASRTL_RECV_ERR, rst );

        // Drain: tick to send request then satisfy it
        check_cntr_tick( &cntr, t++ );
        coll.data.pop_back();
        uint8_t           vbuf[16];
        struct asrtl_span vsp = { .b = vbuf, .e = vbuf + sizeof vbuf };
        asrtl_msg_rtoc_proto_version( 0, 1, 0, asrtl_rec_span_to_span_cb, &vsp );
        check_recv_and_spin( &cntr, vbuf, vsp.b, &t );
}

// truncated proto-version reply while in INIT/WAITING
TEST_CASE_FIXTURE( controller_ctx, "cntr_recv_truncated_init" )
{
        enum asrtc_status st =
            asrtc_cntr_init( &cntr, send, asrtc_default_allocator(), asrtc_default_error_cb() );
        CHECK_EQ( ASRTC_SUCCESS, st );
        st = asrtc_cntr_start( &cntr, &record_init_cb, &init_status, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr, t++ );
        coll.data.pop_back();

        // ID + only major(2) — missing minor(2) + patch(2) = too short
        uint8_t           buf[16];
        struct asrtl_span sp = { .b = buf, .e = buf + sizeof buf };
        asrtl_add_u16( &sp.b, ASRTL_MSG_PROTO_VERSION );
        asrtl_add_u16( &sp.b, 0 );  // major only, 4 bytes total
        enum asrtl_status rst =
            asrtc_cntr_recv( &cntr, (struct asrtl_span) { .b = buf, .e = sp.b } );
        CHECK_EQ( ASRTL_RECV_ERR, rst );

        // Satisfy properly to clean up
        sp = (struct asrtl_span) { .b = buf, .e = buf + sizeof buf };
        asrtl_msg_rtoc_proto_version( 0, 1, 0, asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, buf, sp.b, &t );
}

// truncated test-count reply while in HNDL_TC/WAITING
TEST_CASE_FIXTURE( controller_ctx, "cntr_recv_truncated_test_count" )
{
        check_cntr_full_init( this );

        enum asrtc_status cb_st = ASRTC_SUCCESS;
        enum asrtc_status st    = asrtc_cntr_test_count( &cntr, &record_tc_cb, &cb_st, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr, t++ );
        coll.data.pop_back();

        // Just the message ID, no u16 count
        uint8_t           buf[16];
        struct asrtl_span sp = { .b = buf, .e = buf + sizeof buf };
        asrtl_add_u16( &sp.b, ASRTL_MSG_TEST_COUNT );
        enum asrtl_status rst =
            asrtc_cntr_recv( &cntr, (struct asrtl_span) { .b = buf, .e = sp.b } );
        CHECK_EQ( ASRTL_RECV_ERR, rst );

        // Satisfy properly to clean up
        sp = (struct asrtl_span) { .b = buf, .e = buf + sizeof buf };
        asrtl_msg_rtoc_test_count( 0, asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, buf, sp.b, &t );
}

// truncated test-info reply while in HNDL_TI/WAITING
TEST_CASE_FIXTURE( controller_ctx, "cntr_recv_truncated_test_info" )
{
        check_cntr_full_init( this );

        struct test_info_result p  = { 0 };
        enum asrtc_status       st = asrtc_cntr_test_info( &cntr, 7, &cpy_test_info_cb, &p, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr, t++ );
        coll.data.pop_back();

        // Just the message ID, no u16 tid
        uint8_t           buf[16];
        struct asrtl_span sp = { .b = buf, .e = buf + sizeof buf };
        asrtl_add_u16( &sp.b, ASRTL_MSG_TEST_INFO );
        enum asrtl_status rst =
            asrtc_cntr_recv( &cntr, (struct asrtl_span) { .b = buf, .e = sp.b } );
        CHECK_EQ( ASRTL_RECV_ERR, rst );

        // Satisfy properly to clean up
        char const* desc = "x";
        sp               = (struct asrtl_span) { .b = buf, .e = buf + sizeof buf };
        asrtl_msg_rtoc_test_info( 7, desc, strlen( desc ), asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, buf, sp.b, &t );
}

//---------------------------------------------------------------------
// diag channel — controller side

static inline enum asrtl_status call_diag_recv( struct asrtc_diag* d, uint8_t* b, uint8_t* e )
{
        return d->node.recv_cb( d->node.recv_ptr, (struct asrtl_span) { .b = b, .e = e } );
}

struct diag_ctx
{
        struct asrtl_node  head      = {};
        stub_allocator_ctx alloc_ctx = {};
        asrtl_allocator    alloc     = {};
        asrtc_diag         diag      = {};
        asrtl_sender       null_send = {};

        diag_ctx()
        {
                head.chid = ASRTL_CORE;
                alloc     = asrtl_stub_allocator( &alloc_ctx );
                REQUIRE_EQ( ASRTC_SUCCESS, asrtc_diag_init( &diag, &head, null_send, alloc ) );
        }
};

TEST_CASE( "diag_init" )
{
        struct asrtl_node head = {};
        head.chid              = ASRTL_CORE;
        asrtl_sender null_send = {};

        // diag = NULL
        CHECK_EQ(
            ASRTC_CNTR_INIT_ERR,
            asrtc_diag_init( NULL, &head, null_send, asrtc_default_allocator() ) );

        // prev = NULL
        struct asrtc_diag diag = {};
        CHECK_EQ(
            ASRTC_CNTR_INIT_ERR,
            asrtc_diag_init( &diag, NULL, null_send, asrtc_default_allocator() ) );

        // valid
        CHECK_EQ(
            ASRTC_SUCCESS, asrtc_diag_init( &diag, &head, null_send, asrtc_default_allocator() ) );
        CHECK_EQ( ASRTL_DIAG, diag.node.chid );
        CHECK_NE( nullptr, (void*) (uintptr_t) diag.node.recv_cb );
        CHECK_EQ( nullptr, diag.first_rec );
        CHECK_EQ( nullptr, diag.last_rec );
        CHECK_EQ( &diag.node, head.next );
        CHECK_EQ( nullptr, diag.node.next );
        asrtc_diag_deinit( &diag );
}

TEST_CASE_FIXTURE( diag_ctx, "diag_recv" )
{
        uint8_t  buf[64];
        uint8_t* p;

        // empty buffer → SUCCESS, no record queued
        CHECK_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, buf ) );
        CHECK_EQ( nullptr, diag.first_rec );

        // valid RECORD line=7, file="foo"
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 7 );
        *p++ = 3;  // file_len
        *p++ = 'f';
        *p++ = 'o';
        *p++ = 'o';
        CHECK_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, p ) );
        {
                auto* rec = asrtc_diag_take_record( &diag );
                REQUIRE_NE( nullptr, rec );
                CHECK_EQ( 7u, rec->line );
                REQUIRE_NE( nullptr, rec->file );
                CHECK_EQ( 0, strcmp( rec->file, "foo" ) );
                asrtc_diag_free_record( &diag.alloc, rec );
        }

        // empty filename → file is empty string
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 42 );
        *p++ = 0;  // file_len
        CHECK_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, p ) );
        {
                auto* rec = asrtc_diag_take_record( &diag );
                REQUIRE_NE( nullptr, rec );
                CHECK_EQ( 42u, rec->line );
                REQUIRE_NE( nullptr, rec->file );
                CHECK_EQ( 0u, strlen( rec->file ) );
                asrtc_diag_free_record( &diag.alloc, rec );
        }

        // two RECORDs → FIFO order
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 1 );
        *p++ = 1;  // file_len
        *p++ = 'a';
        CHECK_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, p ) );
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 2 );
        *p++ = 1;  // file_len
        *p++ = 'b';
        CHECK_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, p ) );
        {
                auto* r1 = asrtc_diag_take_record( &diag );
                auto* r2 = asrtc_diag_take_record( &diag );
                REQUIRE_NE( nullptr, r1 );
                REQUIRE_NE( nullptr, r2 );
                CHECK_EQ( 1u, r1->line );
                CHECK_EQ( 2u, r2->line );
                asrtc_diag_free_record( &diag.alloc, r1 );
                asrtc_diag_free_record( &diag.alloc, r2 );
        }

        asrtc_diag_deinit( &diag );
}

TEST_CASE_FIXTURE( diag_ctx, "diag_recv_errors" )
{
        uint8_t buf[8];

        // msgid present but line is truncated (only 3 of 4 bytes)
        uint8_t* p = buf;
        *p++       = ASRTL_DIAG_MSG_RECORD;
        *p++       = 0x00;
        *p++       = 0x00;
        *p++       = 0x07;  // 3 bytes for line, need 4
        CHECK_EQ( ASRTL_RECV_ERR, call_diag_recv( &diag, buf, p ) );
        CHECK_EQ( nullptr, diag.first_rec );

        // unknown ID 0x00
        buf[0] = 0x00;
        CHECK_EQ( ASRTL_RECV_UNEXPECTED_ERR, call_diag_recv( &diag, buf, buf + 1 ) );
        CHECK_EQ( nullptr, diag.first_rec );

        // unknown ID 0xFF
        buf[0] = 0xFF;
        CHECK_EQ( ASRTL_RECV_UNEXPECTED_ERR, call_diag_recv( &diag, buf, buf + 1 ) );
        CHECK_EQ( nullptr, diag.first_rec );

        asrtc_diag_deinit( &diag );
}

TEST_CASE_FIXTURE( diag_ctx, "diag_recv_alloc_failure" )
{
        uint8_t  buf[8];
        uint8_t* p;

        // first alloc (rec struct) fails
        alloc_ctx.fail_at_call = 1;
        p                      = buf;
        *p++                   = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 10 );
        *p++ = 1;  // file_len
        *p++ = 'x';
        CHECK_EQ( ASRTL_ALLOC_ERR, call_diag_recv( &diag, buf, p ) );
        CHECK_EQ( nullptr, diag.first_rec );
        CHECK_EQ( 0u, alloc_ctx.free_calls );

        // second alloc (file string) fails; rec is freed
        alloc_ctx.alloc_calls  = 0;
        alloc_ctx.free_calls   = 0;
        alloc_ctx.fail_at_call = 2;
        p                      = buf;
        *p++                   = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 20 );
        *p++ = 1;  // file_len
        *p++ = 'y';
        CHECK_EQ( ASRTL_ALLOC_ERR, call_diag_recv( &diag, buf, p ) );
        CHECK_EQ( nullptr, diag.first_rec );
        CHECK_EQ( 1u, alloc_ctx.free_calls );

        alloc_ctx.fail_at_call = 0;
        asrtc_diag_deinit( &diag );
}

TEST_CASE_FIXTURE( diag_ctx, "diag_take_record" )
{
        uint8_t  buf[8];
        uint8_t* p;

        // NULL diag
        CHECK_EQ( nullptr, asrtc_diag_take_record( NULL ) );

        // empty queue
        CHECK_EQ( nullptr, asrtc_diag_take_record( &diag ) );

        // one record → returned, queue empty after
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 99 );
        *p++ = 1;  // file_len
        *p++ = 'z';
        REQUIRE_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, p ) );
        {
                auto* rec = asrtc_diag_take_record( &diag );
                REQUIRE_NE( nullptr, rec );
                CHECK_EQ( 99u, rec->line );
                CHECK_EQ( nullptr, diag.first_rec );
                CHECK_EQ( nullptr, diag.last_rec );
                asrtc_diag_free_record( &diag.alloc, rec );
        }

        // C-TAKE-4,5: three records A(1) B(2) C(3) → FIFO order
        for ( uint32_t i = 1; i <= 3; i++ ) {
                p    = buf;
                *p++ = ASRTL_DIAG_MSG_RECORD;
                asrtl_add_u32( &p, i );
                *p++ = 1;  // file_len
                *p++ = 'a';
                REQUIRE_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, p ) );
        }
        for ( uint32_t i = 1; i <= 3; i++ ) {
                auto* rec = asrtc_diag_take_record( &diag );
                REQUIRE_NE( nullptr, rec );
                CHECK_EQ( i, rec->line );
                asrtc_diag_free_record( &diag.alloc, rec );
        }

        // queue empty; take returns NULL
        CHECK_EQ( nullptr, asrtc_diag_take_record( &diag ) );

        // take, insert, take → last_rec stays consistent
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 10 );
        *p++ = 1;  // file_len
        *p++ = 'x';
        REQUIRE_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, p ) );
        auto* rec_a = asrtc_diag_take_record( &diag );
        REQUIRE_NE( nullptr, rec_a );
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 20 );
        *p++ = 1;  // file_len
        *p++ = 'y';
        REQUIRE_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, p ) );
        auto* rec_b = asrtc_diag_take_record( &diag );
        REQUIRE_NE( nullptr, rec_b );
        CHECK_EQ( 20u, rec_b->line );
        CHECK_EQ( nullptr, diag.first_rec );
        CHECK_EQ( nullptr, diag.last_rec );
        asrtc_diag_free_record( &diag.alloc, rec_a );
        asrtc_diag_free_record( &diag.alloc, rec_b );

        asrtc_diag_deinit( &diag );
}

// C-FREE-1,2
TEST_CASE( "diag_free_record" )
{
        struct asrtl_node head       = {};
        head.chid                    = ASRTL_CORE;
        stub_allocator_ctx sctx      = {};
        asrtl_allocator    alloc     = asrtl_stub_allocator( &sctx );
        asrtl_sender       null_send = {};
        struct asrtc_diag  diag      = {};
        REQUIRE_EQ( ASRTC_SUCCESS, asrtc_diag_init( &diag, &head, null_send, alloc ) );

        uint8_t  buf[16];
        uint8_t* p = buf;
        *p++       = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 5 );
        *p++ = 2;  // file_len
        *p++ = 'h';
        *p++ = 'i';
        REQUIRE_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, p ) );

        // record with non-NULL file → two free calls
        auto* rec = asrtc_diag_take_record( &diag );
        REQUIRE_NE( nullptr, rec );
        REQUIRE_NE( nullptr, rec->file );
        sctx.free_calls = 0;
        asrtc_diag_free_record( &alloc, rec );
        CHECK_EQ( 2u, sctx.free_calls );

        // record with file == NULL → one free call
        auto* rec2 =
            static_cast< asrtc_diag_record* >( asrtl_alloc( &alloc, sizeof( asrtc_diag_record ) ) );
        REQUIRE_NE( nullptr, rec2 );
        *rec2           = { .file = nullptr, .extra = nullptr, .line = 0, .next = nullptr };
        sctx.free_calls = 0;
        asrtc_diag_free_record( &alloc, rec2 );
        CHECK_EQ( 1u, sctx.free_calls );

        asrtc_diag_deinit( &diag );
}

// C-DEINIT-1..5
TEST_CASE_FIXTURE( diag_ctx, "diag_deinit" )
{
        uint8_t  buf[8];
        uint8_t* p;

        // NULL → error
        CHECK_EQ( ASRTC_CNTR_INIT_ERR, asrtc_diag_deinit( NULL ) );

        // empty queue → success
        {
                struct asrtl_node head2      = {};
                head2.chid                   = ASRTL_CORE;
                asrtl_sender      null_send2 = {};
                struct asrtc_diag d2         = {};
                asrtc_diag_init( &d2, &head2, null_send2, asrtc_default_allocator() );
                CHECK_EQ( ASRTC_SUCCESS, asrtc_diag_deinit( &d2 ) );
        }

        // one record → freed
        {
                struct asrtl_node head3       = {};
                head3.chid                    = ASRTL_CORE;
                stub_allocator_ctx sctx3      = {};
                asrtl_allocator    a3         = asrtl_stub_allocator( &sctx3 );
                asrtl_sender       null_send3 = {};
                struct asrtc_diag  d3         = {};
                asrtc_diag_init( &d3, &head3, null_send3, a3 );
                p    = buf;
                *p++ = ASRTL_DIAG_MSG_RECORD;
                asrtl_add_u32( &p, 1 );
                *p++ = 1;  // file_len
                *p++ = 'a';
                REQUIRE_EQ( ASRTL_SUCCESS, call_diag_recv( &d3, buf, p ) );
                CHECK_EQ( ASRTC_SUCCESS, asrtc_diag_deinit( &d3 ) );
                CHECK_EQ( nullptr, d3.first_rec );
                CHECK_EQ( 2u, sctx3.free_calls );  // file + rec
        }

        // three records → all freed
        {
                struct asrtl_node head4       = {};
                head4.chid                    = ASRTL_CORE;
                stub_allocator_ctx sctx4      = {};
                asrtl_allocator    a4         = asrtl_stub_allocator( &sctx4 );
                asrtl_sender       null_send4 = {};
                struct asrtc_diag  d4         = {};
                asrtc_diag_init( &d4, &head4, null_send4, a4 );
                for ( uint32_t i = 0; i < 3; i++ ) {
                        p    = buf;
                        *p++ = ASRTL_DIAG_MSG_RECORD;
                        asrtl_add_u32( &p, i );
                        *p++ = 1;  // file_len
                        *p++ = 'x';
                        REQUIRE_EQ( ASRTL_SUCCESS, call_diag_recv( &d4, buf, p ) );
                }
                CHECK_EQ( ASRTC_SUCCESS, asrtc_diag_deinit( &d4 ) );
                CHECK_EQ( nullptr, d4.first_rec );
                CHECK_EQ( 6u, sctx4.free_calls );  // 3 × (file + rec)
        }

        // partial take then deinit frees remaining
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 1 );
        *p++ = 1;  // file_len
        *p++ = 'a';
        REQUIRE_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, p ) );
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 2 );
        *p++ = 1;  // file_len
        *p++ = 'b';
        REQUIRE_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, p ) );
        auto* taken = asrtc_diag_take_record( &diag );
        REQUIRE_NE( nullptr, taken );
        asrtc_diag_free_record( &diag.alloc, taken );
        CHECK_EQ( ASRTC_SUCCESS, asrtc_diag_deinit( &diag ) );
        CHECK_EQ( nullptr, diag.first_rec );
}

// truncated exec messages while in HNDL_EXEC/WAITING
TEST_CASE_FIXTURE( controller_ctx, "cntr_recv_truncated_exec" )
{
        check_cntr_full_init( this );

        struct asrtc_result res = { 0 };
        enum asrtc_status   st  = asrtc_cntr_test_exec( &cntr, 1, result_cb, &res, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr, t++ );
        coll.data.pop_back();

        uint8_t           buf[16];
        struct asrtl_span sp;

        // Truncated TEST_RESULT: ID + run_id(4) only — missing res(2)
        sp = (struct asrtl_span) { .b = buf, .e = buf + sizeof buf };
        asrtl_add_u16( &sp.b, ASRTL_MSG_TEST_RESULT );
        asrtl_add_u32( &sp.b, 0 );  // run_id, but no res u16
        enum asrtl_status rst =
            asrtc_cntr_recv( &cntr, (struct asrtl_span) { .b = buf, .e = sp.b } );
        CHECK_EQ( ASRTL_RECV_ERR, rst );

        // Satisfy properly to clean up
        sp = (struct asrtl_span) { .b = buf, .e = buf + sizeof buf };
        asrtl_msg_rtoc_test_result( 0, ASRTL_TEST_SUCCESS, asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, buf, sp.b, &t );
        CHECK_EQ( ASRTC_TEST_SUCCESS, res.res );
        CHECK_EQ( coll.data.empty(), true );
}

// ---------------------------------------------------------------------------
// diag: protocol-level round-trip tests (expose file/extra parsing bug)

TEST_CASE_FIXTURE( diag_ctx, "diag_recv_proto_no_extra" )
{
        // Serialize with the real protocol encoder (extra=NULL)
        uint8_t           buf[128];
        struct asrtl_span sp = { .b = buf, .e = buf + sizeof buf };
        asrtl_msg_rtoc_diag_record( "foo.c", 7, NULL, asrtl_rec_span_to_span_cb, &sp );

        CHECK_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, sp.b ) );

        auto* rec = asrtc_diag_take_record( &diag );
        REQUIRE_NE( nullptr, rec );
        CHECK_EQ( 7u, rec->line );
        REQUIRE_NE( nullptr, rec->file );
        CHECK_EQ( std::string( "foo.c" ), std::string( rec->file ) );
        asrtc_diag_free_record( &diag.alloc, rec );

        asrtc_diag_deinit( &diag );
}

TEST_CASE_FIXTURE( diag_ctx, "diag_recv_proto_with_extra" )
{
        // Serialize with the real protocol encoder, file + extra
        uint8_t           buf[128];
        struct asrtl_span sp = { .b = buf, .e = buf + sizeof buf };
        asrtl_msg_rtoc_diag_record( "test.c", 42, "x > 0", asrtl_rec_span_to_span_cb, &sp );

        CHECK_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, sp.b ) );

        auto* rec = asrtc_diag_take_record( &diag );
        REQUIRE_NE( nullptr, rec );
        CHECK_EQ( 42u, rec->line );
        REQUIRE_NE( nullptr, rec->file );
        // file must be exactly "test.c", not polluted by the file_len prefix or extra
        CHECK_EQ( std::string( "test.c" ), std::string( rec->file ) );
        asrtc_diag_free_record( &diag.alloc, rec );

        asrtc_diag_deinit( &diag );
}

// ============================================================================
// asrtc_param_server — controller PARAM channel (Phase 2)
// ============================================================================

static struct asrtl_flat_tree make_param_tree()
{
        struct asrtl_allocator alloc = asrtl_default_allocator();
        struct asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, alloc, 8, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, NULL, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "alpha", asrtl_flat_value_u32( 10 ) );
        asrtl_flat_tree_append( &tree, 1, 3, "beta", asrtl_flat_value_str( "hi" ) );
        asrtl_flat_tree_append( &tree, 1, 4, "gamma", asrtl_flat_value_bool( 1 ) );
        return tree;
}

static inline enum asrtl_status call_param_recv(
    struct asrtc_param_server* p,
    uint8_t*                   b,
    uint8_t*                   e )
{
        return p->node.recv_cb( p->node.recv_ptr, (struct asrtl_span) { .b = b, .e = e } );
}

// Build a raw READY_ACK message into buf; return past-end pointer.
static uint8_t* build_ready_ack( uint8_t* buf, uint32_t max_msg_size )
{
        uint8_t* p = buf;
        *p++       = ASRTL_PARAM_MSG_READY_ACK;
        asrtl_add_u32( &p, max_msg_size );
        return p;
}

// Build a raw QUERY message into buf; return past-end pointer.
static uint8_t* build_query( uint8_t* buf, asrtl_flat_id node_id )
{
        uint8_t* p = buf;
        *p++       = ASRTL_PARAM_MSG_QUERY;
        asrtl_add_u32( &p, node_id );
        return p;
}

struct param_ctx
{
        struct asrtl_node         head      = {};
        stub_allocator_ctx        alloc_ctx = {};
        asrtl_allocator           alloc     = {};
        struct asrtc_param_server param     = {};
        collector                 coll;
        asrtl_sender              sendr = {};
        uint32_t                  t     = 1;

        param_ctx()
        {
                head.chid = ASRTL_CORE;
                alloc     = asrtl_stub_allocator( &alloc_ctx );
                setup_sender_collector( &sendr, &coll );
                REQUIRE_EQ( ASRTC_SUCCESS, asrtc_param_server_init( &param, &head, sendr, alloc ) );
        }
        ~param_ctx()
        {
                asrtc_param_server_deinit( &param );
        }
};

TEST_CASE( "asrtc_param_server_init" )
{
        struct asrtl_node head           = {};
        head.chid                        = ASRTL_CORE;
        asrtl_sender              null_s = {};
        struct asrtc_param_server param2 = {};

        CHECK_EQ(
            ASRTC_CNTR_INIT_ERR,
            asrtc_param_server_init( NULL, &head, null_s, asrtl_default_allocator() ) );
        CHECK_EQ(
            ASRTC_CNTR_INIT_ERR,
            asrtc_param_server_init( &param2, NULL, null_s, asrtl_default_allocator() ) );

        CHECK_EQ(
            ASRTC_SUCCESS,
            asrtc_param_server_init( &param2, &head, null_s, asrtl_default_allocator() ) );
        CHECK_EQ( ASRTL_PARAM, param2.node.chid );
        CHECK_NE( nullptr, (void*) (uintptr_t) param2.node.recv_cb );
        CHECK_EQ( &param2.node, head.next );
        CHECK_EQ( nullptr, param2.tree );
        CHECK_EQ( nullptr, param2.enc_buff );
        CHECK_EQ( 0, param2.ack_received );
        asrtc_param_server_deinit( &param2 );
}

TEST_CASE_FIXTURE( param_ctx, "asrtc_param_server_send_ready_no_tree" )
{
        CHECK_EQ( ASRTL_ARG_ERR, asrtc_param_server_send_ready( &param, 1u, 1000, NULL, NULL ) );
        CHECK( coll.data.empty() );
}

TEST_CASE_FIXTURE( param_ctx, "asrtc_param_server_send_ready_invalid_id" )
{
        struct asrtl_flat_tree tree = make_param_tree();
        asrtc_param_server_set_tree( &param, &tree );
        CHECK_EQ( ASRTL_ARG_ERR, asrtc_param_server_send_ready( &param, 999u, 1000, NULL, NULL ) );
        CHECK( coll.data.empty() );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_ctx, "asrtc_param_server_send_ready_encodes_correctly" )
{
        struct asrtl_flat_tree tree = make_param_tree();
        asrtc_param_server_set_tree( &param, &tree );

        CHECK_EQ( ASRTL_SUCCESS, asrtc_param_server_send_ready( &param, 1u, 1000, NULL, NULL ) );
        REQUIRE_EQ( 1u, coll.data.size() );

        auto& msg = coll.data.front();
        CHECK_EQ( ASRTL_PARAM, msg.id );
        REQUIRE_EQ( 5u, msg.data.size() );
        CHECK_EQ( ASRTL_PARAM_MSG_READY, msg.data[0] );
        // root_id = 1 as big-endian u32
        CHECK_EQ( 0u, msg.data[1] );
        CHECK_EQ( 0u, msg.data[2] );
        CHECK_EQ( 0u, msg.data[3] );
        CHECK_EQ( 1u, msg.data[4] );

        CHECK_EQ( 0, param.ack_received );  // reset on send_ready
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_ctx, "asrtc_param_server_recv_ready_ack_allocates_buffer" )
{
        uint8_t buf[8];
        CHECK_EQ( ASRTL_SUCCESS, call_param_recv( &param, buf, build_ready_ack( buf, 256u ) ) );
        CHECK_EQ( ASRTL_SUCCESS, asrtc_param_server_tick( &param, t++ ) );
        CHECK_EQ( 1, param.ack_received );
        CHECK_EQ( 256u, param.max_msg_size );
        CHECK_NE( nullptr, param.enc_buff );
        CHECK_EQ( 1u, alloc_ctx.alloc_calls );
}

// BUG: handle_ready_ack does not guard against overwriting an already-pending
// event.  A second recv (without a tick) should return ASRTL_RECV_ERR but
// currently returns ASRTL_SUCCESS, so this test is expected to FAIL.
TEST_CASE_FIXTURE( param_ctx, "asrtc_param_server_recv_ready_ack_while_pending_returns_error" )
{
        uint8_t buf[8];
        CHECK_EQ( ASRTL_SUCCESS, call_param_recv( &param, buf, build_ready_ack( buf, 256u ) ) );
        // pending == PENDING_READY_ACK; tick not called yet
        CHECK_EQ( ASRTL_RECV_ERR, call_param_recv( &param, buf, build_ready_ack( buf, 512u ) ) );
        // Consume the first pending so the fixture destructor stays clean
        asrtc_param_server_tick( &param, t++ );
}

// BUG: handle_query does not guard against overwriting an already-pending
// event.  A second recv (without a tick) should return ASRTL_RECV_ERR but
// currently returns ASRTL_SUCCESS, so this test is expected to FAIL.
TEST_CASE_FIXTURE( param_ctx, "asrtc_param_server_recv_query_while_pending_returns_error" )
{
        struct asrtl_flat_tree tree = make_param_tree();
        asrtc_param_server_set_tree( &param, &tree );

        uint8_t buf[8];
        // Handshake so ack_received=1 and enc_buff is ready
        CHECK_EQ( ASRTL_SUCCESS, call_param_recv( &param, buf, build_ready_ack( buf, 256u ) ) );
        CHECK_EQ( ASRTL_SUCCESS, asrtc_param_server_tick( &param, t++ ) );

        // First query stores pending
        CHECK_EQ( ASRTL_SUCCESS, call_param_recv( &param, buf, build_query( buf, 2u ) ) );
        // pending == PENDING_QUERY; tick not called yet
        CHECK_EQ( ASRTL_RECV_ERR, call_param_recv( &param, buf, build_query( buf, 3u ) ) );
        // Consume the first pending so the fixture destructor stays clean
        asrtc_param_server_tick( &param, t++ );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_ctx, "asrtc_param_server_query_before_ack_returns_error" )
{
        struct asrtl_flat_tree tree = make_param_tree();
        asrtc_param_server_set_tree( &param, &tree );

        uint8_t buf[8];
        CHECK_EQ( ASRTL_RECV_ERR, call_param_recv( &param, buf, build_query( buf, 2u ) ) );
        CHECK( coll.data.empty() );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_ctx, "asrtc_param_server_query_produces_response" )
{
        struct asrtl_flat_tree tree = make_param_tree();
        asrtc_param_server_set_tree( &param, &tree );

        uint8_t buf[8];
        // Handshake
        CHECK_EQ( ASRTL_SUCCESS, call_param_recv( &param, buf, build_ready_ack( buf, 256u ) ) );
        CHECK_EQ( ASRTL_SUCCESS, asrtc_param_server_tick( &param, t++ ) );

        // Query node 2 ("alpha", u32=10)
        CHECK_EQ( ASRTL_SUCCESS, call_param_recv( &param, buf, build_query( buf, 2u ) ) );
        CHECK_EQ( ASRTL_SUCCESS, asrtc_param_server_tick( &param, t++ ) );
        REQUIRE_EQ( 1u, coll.data.size() );

        auto& msg = coll.data.front();
        CHECK_EQ( ASRTL_PARAM, msg.id );
        CHECK_EQ( ASRTL_PARAM_MSG_RESPONSE, msg.data[0] );

        // Verify wire format: first node id should be 2 (big-endian u32 at offset 1)
        REQUIRE( msg.data.size() > 5u );
        uint32_t first_id = ( (uint32_t) msg.data[1] << 24 ) | ( (uint32_t) msg.data[2] << 16 ) |
                            ( (uint32_t) msg.data[3] << 8 ) | (uint32_t) msg.data[4];
        CHECK_EQ( 2u, first_id );

        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_ctx, "asrtc_param_server_query_multi_batch" )
{
        // Use a small max_msg_size so only one node fits per batch.
        // Node 2: id(4) + "alpha\0"(6) + type(1) + u32(4) = 15 node bytes
        // Full message: msg_id(1) + 15 + next_sib(4) = 20 bytes.
        struct asrtl_flat_tree tree = make_param_tree();
        asrtc_param_server_set_tree( &param, &tree );

        uint8_t buf[8];
        CHECK_EQ( ASRTL_SUCCESS, call_param_recv( &param, buf, build_ready_ack( buf, 20u ) ) );
        CHECK_EQ( ASRTL_SUCCESS, asrtc_param_server_tick( &param, t++ ) );

        // First query: only node 2 fits → next_sibling_id points to node 3
        CHECK_EQ( ASRTL_SUCCESS, call_param_recv( &param, buf, build_query( buf, 2u ) ) );
        CHECK_EQ( ASRTL_SUCCESS, asrtc_param_server_tick( &param, t++ ) );
        REQUIRE_EQ( 1u, coll.data.size() );

        auto& msg = coll.data.front();
        CHECK_EQ( ASRTL_PARAM_MSG_RESPONSE, msg.data[0] );
        REQUIRE_EQ( 20u, msg.data.size() );  // msg_id(1) + node(15) + next_sib(4)
        // First node id = 2
        uint32_t nid = ( (uint32_t) msg.data[1] << 24 ) | ( (uint32_t) msg.data[2] << 16 ) |
                       ( (uint32_t) msg.data[3] << 8 ) | (uint32_t) msg.data[4];
        CHECK_EQ( 2u, nid );
        // Trailing next_sibling_id = 3 (last 4 bytes)
        uint32_t nsib = ( (uint32_t) msg.data[16] << 24 ) | ( (uint32_t) msg.data[17] << 16 ) |
                        ( (uint32_t) msg.data[18] << 8 ) | (uint32_t) msg.data[19];
        CHECK_EQ( 3u, nsib );  // next batch starts at node 3

        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_ctx, "asrtc_param_server_query_oversize_returns_error" )
{
        // max_msg_size=11 — minimum struct size but key "alpha" doesn't fit
        struct asrtl_flat_tree tree = make_param_tree();
        asrtc_param_server_set_tree( &param, &tree );

        uint8_t buf[8];
        CHECK_EQ( ASRTL_SUCCESS, call_param_recv( &param, buf, build_ready_ack( buf, 11u ) ) );
        CHECK_EQ( ASRTL_SUCCESS, asrtc_param_server_tick( &param, t++ ) );
        CHECK_EQ( ASRTL_SUCCESS, call_param_recv( &param, buf, build_query( buf, 2u ) ) );
        CHECK_EQ( ASRTL_SUCCESS, asrtc_param_server_tick( &param, t++ ) );

        REQUIRE_EQ( 1u, coll.data.size() );
        auto& msg = coll.data.front();
        CHECK_EQ( ASRTL_PARAM, msg.id );
        REQUIRE_EQ( 6u, msg.data.size() );
        CHECK_EQ( ASRTL_PARAM_MSG_ERROR, msg.data[0] );
        CHECK_EQ( ASRTL_PARAM_ERR_RESPONSE_TOO_LARGE, msg.data[1] );
        // node_id = 2 as big-endian u32
        CHECK_EQ( 0u, msg.data[2] );
        CHECK_EQ( 0u, msg.data[3] );
        CHECK_EQ( 0u, msg.data[4] );
        CHECK_EQ( 2u, msg.data[5] );

        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_ctx, "asrtc_param_server_deinit_frees_buffer" )
{
        uint8_t buf[8];
        CHECK_EQ( ASRTL_SUCCESS, call_param_recv( &param, buf, build_ready_ack( buf, 64u ) ) );
        CHECK_EQ( ASRTL_SUCCESS, asrtc_param_server_tick( &param, t++ ) );
        CHECK_NE( nullptr, param.enc_buff );
        CHECK_EQ( 1u, alloc_ctx.alloc_calls );
        CHECK_EQ( 0u, alloc_ctx.free_calls );

        asrtc_param_server_deinit( &param );
        CHECK_EQ( nullptr, param.enc_buff );
        CHECK_EQ( 1u, alloc_ctx.free_calls );

        // Prevent double-free in fixture destructor
        alloc_ctx.free_calls = 0;
}

// ============================================================================
// Phase 4 — C loopback integration test (server ↔ client)
// ============================================================================

struct param_loopback_ctx
{
        // Both sides share one head each (CORE placeholder + PARAM node)
        struct asrtl_node         srv_head            = {};
        struct asrtl_node         cli_head            = {};
        stub_allocator_ctx        alloc_ctx           = {};
        asrtl_allocator           alloc               = {};
        struct asrtc_param_server server              = {};
        struct asrtr_param_client client              = {};
        static constexpr uint32_t CLI_BUF_SZ          = 256;
        uint8_t                   cli_buf[CLI_BUF_SZ] = {};

        // Cross-wired senders: server sends → client recv, client sends → server recv
        asrtl_sender srv_sendr = {};
        asrtl_sender cli_sendr = {};

        static enum asrtl_status srv_to_cli(
            void* ptr,
            asrtl_chann_id /*id*/,
            struct asrtl_rec_span* buff )
        {
                auto*             ctx = (param_loopback_ctx*) ptr;
                uint8_t           flat[512];
                struct asrtl_span sp = { .b = flat, .e = flat + sizeof flat };
                asrtl_rec_span_to_span( &sp, buff );
                struct asrtl_span msg = { .b = flat, .e = sp.b };
                return ctx->client.node.recv_cb( ctx->client.node.recv_ptr, msg );
        }

        static enum asrtl_status cli_to_srv(
            void* ptr,
            asrtl_chann_id /*id*/,
            struct asrtl_rec_span* buff )
        {
                auto*             ctx = (param_loopback_ctx*) ptr;
                uint8_t           flat[512];
                struct asrtl_span sp = { .b = flat, .e = flat + sizeof flat };
                asrtl_rec_span_to_span( &sp, buff );
                struct asrtl_span msg = { .b = flat, .e = sp.b };
                return ctx->server.node.recv_cb( ctx->server.node.recv_ptr, msg );
        }

        // Response callback state
        struct received_node
        {
                asrtl_flat_id    id;
                std::string      key;
                asrtl_flat_value value;
                asrtl_flat_id    next_sibling;
        };
        std::vector< received_node > received;
        int                          error_called = 0;
        uint32_t                     t            = 1;
        struct asrtr_param_query     query        = {};

        static void query_cb(
            struct asrtr_param_client*,
            struct asrtr_param_query* q,
            struct asrtl_flat_value   val )
        {
                auto* ctx = (param_loopback_ctx*) q->cb_ptr;
                if ( q->error_code != 0 ) {
                        ctx->error_called++;
                } else {
                        ctx->received.push_back(
                            { q->node_id, q->key ? q->key : "", val, q->next_sibling } );
                }
        }

        // Tick both sides up to N times until neither has pending work
        void spin( int max_iter = 100 )
        {
                for ( int i = 0; i < max_iter; i++ ) {
                        asrtc_param_server_tick( &server, t++ );
                        asrtr_param_client_tick( &client, t++ );
                        if ( client.pending == ASRTR_PARAM_CLIENT_PENDING_NONE &&
                             server.pending == ASRTC_PARAM_SERVER_PENDING_NONE )
                                break;
                }
        }

        param_loopback_ctx()
        {
                srv_head.chid = ASRTL_CORE;
                cli_head.chid = ASRTL_CORE;
                alloc         = asrtl_stub_allocator( &alloc_ctx );
                srv_sendr     = { .ptr = this, .cb = srv_to_cli };
                cli_sendr     = { .ptr = this, .cb = cli_to_srv };
                REQUIRE_EQ(
                    ASRTC_SUCCESS,
                    asrtc_param_server_init( &server, &srv_head, srv_sendr, alloc ) );
                struct asrtl_span mb = { .b = cli_buf, .e = cli_buf + CLI_BUF_SZ };
                REQUIRE_EQ(
                    ASRTR_SUCCESS,
                    asrtr_param_client_init( &client, &cli_head, cli_sendr, mb, 10 ) );
        }

        ~param_loopback_ctx()
        {
                asrtr_param_client_deinit( &client );
                asrtc_param_server_deinit( &server );
        }
};

TEST_CASE_FIXTURE( param_loopback_ctx, "param_loopback_full_tree_traversal" )
{
        // Build 3-level tree:
        //   root (OBJECT, id=1)
        //     ├── "sub" (OBJECT, id=2)
        //     │     ├── "x" (U32=100, id=4)
        //     │     └── "y" (STR="hello", id=5)
        //     └── "b" (BOOL=1, id=3)
        struct asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, NULL, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "sub", asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 3, "b", asrtl_flat_value_bool( 1 ) );
        asrtl_flat_tree_append( &tree, 2, 4, "x", asrtl_flat_value_u32( 100 ) );
        asrtl_flat_tree_append( &tree, 2, 5, "y", asrtl_flat_value_str( "hello" ) );

        asrtc_param_server_set_tree( &server, &tree );

        // Server sends READY
        CHECK_EQ( ASRTL_SUCCESS, asrtc_param_server_send_ready( &server, 1u, 1000, NULL, NULL ) );
        // Spin: READY → client gets root_id, sends READY_ACK → server allocates
        spin();
        CHECK_EQ( 1, client.ready );
        CHECK_EQ( 1u, asrtr_param_client_root_id( &client ) );

        // Recursive traversal: collect all nodes
        // Query root
        CHECK_EQ(
            ASRTL_SUCCESS, asrtr_param_client_query_any( &query, &client, 1u, query_cb, this ) );
        spin();
        REQUIRE_EQ( 1u, received.size() );
        CHECK_EQ( 1u, received[0].id );
        CHECK_EQ( (uint8_t) ASRTL_FLAT_VALUE_TYPE_OBJECT, (uint8_t) received[0].value.type );
        asrtl_flat_id first_child = received[0].value.obj_val.first_child;
        CHECK_NE( 0u, first_child );  // should be 2

        // Query first child of root ("sub", id=2)
        CHECK_EQ(
            ASRTL_SUCCESS,
            asrtr_param_client_query_any( &query, &client, first_child, query_cb, this ) );
        spin();
        REQUIRE_EQ( 2u, received.size() );
        CHECK_EQ( 2u, received[1].id );
        CHECK_EQ( "sub", received[1].key );
        CHECK_EQ( (uint8_t) ASRTL_FLAT_VALUE_TYPE_OBJECT, (uint8_t) received[1].value.type );
        asrtl_flat_id sub_first_child = received[1].value.obj_val.first_child;
        asrtl_flat_id sub_next_sib    = received[1].next_sibling;
        CHECK_NE( 0u, sub_first_child );  // should be 4
        CHECK_NE( 0u, sub_next_sib );     // should be 3

        // Query next sibling of "sub" → "b" (id=3)
        CHECK_EQ(
            ASRTL_SUCCESS,
            asrtr_param_client_query_any( &query, &client, sub_next_sib, query_cb, this ) );
        spin();
        REQUIRE_EQ( 3u, received.size() );
        CHECK_EQ( 3u, received[2].id );
        CHECK_EQ( "b", received[2].key );
        CHECK_EQ( (uint8_t) ASRTL_FLAT_VALUE_TYPE_BOOL, (uint8_t) received[2].value.type );
        CHECK_EQ( 1u, received[2].value.u32_val );
        CHECK_EQ( 0u, received[2].next_sibling );  // last sibling

        // Query children of "sub": "x" (id=4)
        CHECK_EQ(
            ASRTL_SUCCESS,
            asrtr_param_client_query_any( &query, &client, sub_first_child, query_cb, this ) );
        spin();
        REQUIRE_EQ( 4u, received.size() );
        CHECK_EQ( 4u, received[3].id );
        CHECK_EQ( "x", received[3].key );
        CHECK_EQ( (uint8_t) ASRTL_FLAT_VALUE_TYPE_U32, (uint8_t) received[3].value.type );
        CHECK_EQ( 100u, received[3].value.u32_val );
        asrtl_flat_id x_next_sib = received[3].next_sibling;
        CHECK_NE( 0u, x_next_sib );  // should be 5

        // Query "y" (id=5)
        CHECK_EQ(
            ASRTL_SUCCESS,
            asrtr_param_client_query_any( &query, &client, x_next_sib, query_cb, this ) );
        spin();
        REQUIRE_EQ( 5u, received.size() );
        CHECK_EQ( 5u, received[4].id );
        CHECK_EQ( "y", received[4].key );
        CHECK_EQ( (uint8_t) ASRTL_FLAT_VALUE_TYPE_STR, (uint8_t) received[4].value.type );
        CHECK_EQ( 0u, received[4].next_sibling );  // last sibling

        // No errors throughout
        CHECK_EQ( 0, error_called );

        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "param_loopback_multi_batch" )
{
        // Small buffer (25 bytes) forces server to split siblings across batches.
        // Node wire size for "cN" U32 child: id(4)+"cN\0"(3)+type(1)+val(4) = 12
        // max_msg_size=25: msg_id(1) + 20 node space + 4 trailer → fits 1 node (12<20).
        // Actually 2×12=24>20 so only 1 fits per batch.  4 children → 4 batches.
        // Use 30 to fit 2: 30-1-4=25, 2×12=24<25 → 2 per batch, 4 children → 2 batches.
        static constexpr uint32_t SMALL_BUF = 30;

        struct asrtl_node         srv_head           = {};
        struct asrtl_node         cli_head           = {};
        stub_allocator_ctx        alloc_ctx          = {};
        asrtl_allocator           alloc              = {};
        struct asrtc_param_server server             = {};
        struct asrtr_param_client client             = {};
        uint8_t                   cli_buf[SMALL_BUF] = {};

        struct loopback_ptrs
        {
                asrtc_param_server* server;
                asrtr_param_client* client;
        } cross = { &server, &client };

        auto srv_to_cli =
            []( void* ptr, asrtl_chann_id, struct asrtl_rec_span* buff )->enum asrtl_status
        {
                auto*             c = (loopback_ptrs*) ptr;
                uint8_t           flat[512];
                struct asrtl_span sp = { .b = flat, .e = flat + sizeof flat };
                asrtl_rec_span_to_span( &sp, buff );
                struct asrtl_span msg = { .b = flat, .e = sp.b };
                return c->client->node.recv_cb( c->client->node.recv_ptr, msg );
        };

        auto cli_to_srv =
            []( void* ptr, asrtl_chann_id, struct asrtl_rec_span* buff )->enum asrtl_status
        {
                auto*             c = (loopback_ptrs*) ptr;
                uint8_t           flat[512];
                struct asrtl_span sp = { .b = flat, .e = flat + sizeof flat };
                asrtl_rec_span_to_span( &sp, buff );
                struct asrtl_span msg = { .b = flat, .e = sp.b };
                return c->server->node.recv_cb( c->server->node.recv_ptr, msg );
        };

        srv_head.chid = ASRTL_CORE;
        cli_head.chid = ASRTL_CORE;
        alloc         = asrtl_stub_allocator( &alloc_ctx );

        asrtl_sender ssend = { .ptr = &cross, .cb = srv_to_cli };
        asrtl_sender csend = { .ptr = &cross, .cb = cli_to_srv };

        REQUIRE_EQ( ASRTC_SUCCESS, asrtc_param_server_init( &server, &srv_head, ssend, alloc ) );
        struct asrtl_span mb = { .b = cli_buf, .e = cli_buf + SMALL_BUF };
        REQUIRE_EQ( ASRTR_SUCCESS, asrtr_param_client_init( &client, &cli_head, csend, mb, 100 ) );

        // Tree: root(OBJECT,1) → 4 children (U32, ids 2..5)
        struct asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, NULL, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "c1", asrtl_flat_value_u32( 10 ) );
        asrtl_flat_tree_append( &tree, 1, 3, "c2", asrtl_flat_value_u32( 20 ) );
        asrtl_flat_tree_append( &tree, 1, 4, "c3", asrtl_flat_value_u32( 30 ) );
        asrtl_flat_tree_append( &tree, 1, 5, "c4", asrtl_flat_value_u32( 40 ) );
        asrtc_param_server_set_tree( &server, &tree );

        uint32_t t_   = 1;
        auto     spin = [&] {
                for ( int i = 0; i < 100; i++ ) {
                        asrtc_param_server_tick( &server, t_++ );
                        asrtr_param_client_tick( &client, t_++ );
                        if ( client.pending == ASRTR_PARAM_CLIENT_PENDING_NONE &&
                             server.pending == ASRTC_PARAM_SERVER_PENDING_NONE )
                                break;
                }
        };

        // Handshake
        CHECK_EQ( ASRTL_SUCCESS, asrtc_param_server_send_ready( &server, 1u, 1000, NULL, NULL ) );
        spin();
        CHECK_EQ( 1, client.ready );
        struct rn
        {
                asrtl_flat_id id;
                uint32_t      val;
                asrtl_flat_id next_sib;
        };
        std::vector< rn >        results;
        struct asrtr_param_query q = {};

        auto resp_cb = []( struct asrtr_param_client*,
                           struct asrtr_param_query* qq,
                           struct asrtl_flat_value   val ) {
                auto* r = (std::vector< rn >*) qq->cb_ptr;
                r->push_back( { qq->node_id, val.u32_val, qq->next_sibling } );
        };

        // Query root to get first_child
        asrtl_flat_id first_child = 0;
        auto          root_resp   = []( struct asrtr_param_client*,
                             struct asrtr_param_query* qq,
                             struct asrtl_flat_value   val ) {
                *(asrtl_flat_id*) qq->cb_ptr = val.obj_val.first_child;
        };
        CHECK_EQ(
            ASRTL_SUCCESS,
            asrtr_param_client_query_any( &q, &client, 1u, root_resp, &first_child ) );
        spin();
        REQUIRE_NE( 0u, first_child );

        // Walk all children via next_sibling_id chain
        asrtl_flat_id query_id = first_child;
        while ( query_id != 0u ) {
                CHECK_EQ(
                    ASRTL_SUCCESS,
                    asrtr_param_client_query_any( &q, &client, query_id, resp_cb, &results ) );
                spin();
                REQUIRE_FALSE( results.empty() );
                query_id = results.back().next_sib;
        }

        // Verify all 4 children received in order
        REQUIRE_EQ( 4u, results.size() );
        CHECK_EQ( 2u, results[0].id );
        CHECK_EQ( 10u, results[0].val );
        CHECK_EQ( 3u, results[1].id );
        CHECK_EQ( 20u, results[1].val );
        CHECK_EQ( 4u, results[2].id );
        CHECK_EQ( 30u, results[2].val );
        CHECK_EQ( 5u, results[3].id );
        CHECK_EQ( 40u, results[3].val );
        CHECK_EQ( 0u, results[3].next_sib );

        asrtr_param_client_deinit( &client );
        asrtc_param_server_deinit( &server );
        asrtl_flat_tree_deinit( &tree );
}
