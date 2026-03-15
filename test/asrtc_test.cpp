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
#include "../asrtc/allocator.h"
#include "../asrtc/controller.h"
#include "../asrtc/default_allocator.h"
#include "../asrtc/default_error_cb.h"
#include "../asrtc/diag.h"
#include "../asrtl/core_proto.h"
#include "../asrtl/log.h"
#include "../asrtl/proto_version.h"
#include "../asrtl/util.h"
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

void check_cntr_tick( struct asrtc_controller* c )
{
        enum asrtc_status st = asrtc_cntr_tick( c );
        CHECK_EQ( ASRTC_SUCCESS, st );
}

void check_recv_and_spin( struct asrtc_controller* c, uint8_t* beg, uint8_t* end )
{
        check_cntr_recv( c, (struct asrtl_span) { .b = beg, .e = end } );
        int       i = 0;
        int const n = 1000;
        for ( ; i < n && !asrtc_cntr_idle( c ); i++ )
                check_cntr_tick( c );
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
            &ctx->cntr,
            ctx->send,
            asrtc_default_allocator(),
            asrtc_default_error_cb(),
            &record_init_cb,
            &ctx->init_status,
            0 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &ctx->cntr );

        assert_collected_core_hdr( ctx->coll.data.back(), 0x02, ASRTL_MSG_PROTO_VERSION );
        ctx->coll.data.pop_back();

        uint8_t           buffer[64];
        struct asrtl_span sp = {
            .b = buffer,
            .e = buffer + sizeof buffer,
        };

        asrtl_msg_rtoc_proto_version( 0, 1, 0, asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &ctx->cntr, buffer, sp.b );
        CHECK_EQ( ASRTC_SUCCESS, ctx->init_status );
}


//---------------------------------------------------------------------
// tests

TEST_CASE_FIXTURE( controller_ctx, "cntr_init" )
{
        enum asrtc_status st;
        st = asrtc_cntr_init(
            NULL,
            send,
            asrtc_default_allocator(),
            asrtc_default_error_cb(),
            &record_init_cb,
            NULL,
            0 );
        CHECK_EQ( ASRTC_CNTR_INIT_ERR, st );

        st = asrtc_cntr_init(
            &cntr,
            send,
            asrtc_default_allocator(),
            asrtc_default_error_cb(),
            &record_init_cb,
            &init_status,
            0 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        CHECK_EQ( ASRTL_CORE, cntr.node.chid );
        CHECK_EQ( ASRTC_CNTR_INIT, cntr.state );

        CHECK( !asrtc_cntr_idle( &cntr ) );

        st = asrtc_cntr_tick( &cntr );
        CHECK_EQ( ASRTC_SUCCESS, st );

        assert_collected_core_hdr( coll.data.back(), 0x02, ASRTL_MSG_PROTO_VERSION );
        coll.data.pop_back();

        asrtl_msg_rtoc_proto_version( 0, 1, 0, asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, buffer, sp.b );

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
        st = asrtc_cntr_desc( &cntr, &cpy_desc_cb, (void*) &desc, 0 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr );

        assert_collected_core_hdr( coll.data.back(), 0x02, ASRTL_MSG_DESC );
        coll.data.pop_back();

        char const* msg = "wololo1";
        asrtl_msg_rtoc_desc( msg, strlen( msg ), asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, buffer, sp.b );

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
        st         = asrtc_cntr_test_count( &cntr, &cpy_u32_cb, (void*) &p, 0 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr );

        assert_collected_core_hdr( coll.data.back(), 0x02, ASRTL_MSG_TEST_COUNT );
        coll.data.pop_back();

        asrtl_msg_rtoc_test_count( 42, asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, buffer, sp.b );

        CHECK_EQ( 42, p );
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_test_info" )
{
        enum asrtc_status st;
        check_cntr_full_init( this );

        struct test_info_result p = { 0 };
        st = asrtc_cntr_test_info( &cntr, 42, &cpy_test_info_cb, (void*) &p, 0 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr );

        assert_collected_core_hdr( coll.data.back(), 0x04, ASRTL_MSG_TEST_INFO );
        assert_u16( 42, coll.data.back().data.data() + 2 );
        coll.data.pop_back();

        char const* desc = "barbaz";
        asrtl_msg_rtoc_test_info( 42, desc, strlen( desc ), asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, buffer, sp.b );

        CHECK( desc == p.desc );
        CHECK_EQ( 42, p.tid );
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_test_info_tid_mismatch" )
{
        check_cntr_full_init( this );

        struct test_info_result p = { 0 };
        enum asrtc_status st = asrtc_cntr_test_info( &cntr, 42, &cpy_test_info_cb, (void*) &p, 0 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr );
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
static struct asrtc_allocator failing_allocator( void )
{
        return (struct asrtc_allocator) {
            .ptr   = NULL,
            .alloc = &failing_alloc,
            .free  = &failing_free,
        };
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_desc_alloc_failure" )
{
        enum asrtc_status st = asrtc_cntr_init(
            &cntr,
            send,
            failing_allocator(),
            asrtc_default_error_cb(),
            &record_init_cb,
            &init_status,
            0 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr );
        coll.data.pop_back();  // discard PROTO_VERSION request

        // Simulate receiving a proto-version reply to advance to IDLE
        uint8_t           buf[64];
        struct asrtl_span sp = { .b = buf, .e = buf + sizeof buf };
        asrtl_msg_rtoc_proto_version( 0, 1, 0, asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, buf, sp.b );
        CHECK_EQ( ASRTC_SUCCESS, init_status );

        std::string desc;
        st = asrtc_cntr_desc( &cntr, &cpy_desc_cb, (void*) &desc, 0 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr );
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
        st = asrtc_cntr_test_exec( &cntr, 42, result_cb, &res, 0 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr );

        assert_collected_core_hdr( coll.data.back(), 0x08, ASRTL_MSG_TEST_START );
        assert_u16( 42, coll.data.back().data.data() + 2 );
        assert_u32( 0, coll.data.back().data.data() + 4 );
        coll.data.pop_back();

        for ( int i = 0; i < 4; i++ )
                check_cntr_tick( &cntr );

        asrtl_msg_rtoc_test_start( 42, 0, asrtl_rec_span_to_span_cb, &sp );
        check_cntr_recv( &cntr, (struct asrtl_span) { .b = buffer, .e = sp.b } );
        for ( int i = 0; i < 4; i++ )
                check_cntr_tick( &cntr );

        CHECK_EQ( coll.data.empty(), true );

        uint8_t* b = sp.b;
        asrtl_msg_rtoc_test_result( 0, ASRTL_TEST_SUCCESS, asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, b, sp.b );

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
        struct asrtc_allocator alloc = asrtc_default_allocator();
        char*                  res   = asrtc_realloc_str( &alloc, &sp );
        CHECK_NE( res, nullptr );
        CHECK_EQ( 'x', res[0] );
        CHECK_EQ( '\0', res[len] );
        free( res );
        free( data );
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_version_mismatch" )
{
        enum asrtc_status st = asrtc_cntr_init(
            &cntr,
            send,
            asrtc_default_allocator(),
            asrtc_default_error_cb(),
            &record_init_cb,
            &init_status,
            0 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr );
        coll.data.pop_back();

        // reply with a mismatched major version
        uint8_t           buf[64];
        struct asrtl_span sp = { .b = buf, .e = buf + sizeof buf };
        asrtl_msg_rtoc_proto_version( ASRTL_PROTO_MAJOR + 1, 0, 0, asrtl_rec_span_to_span_cb, &sp );
        check_cntr_recv( &cntr, (struct asrtl_span) { .b = buf, .e = sp.b } );

        st = asrtc_cntr_tick( &cntr );
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
        enum asrtc_status st = asrtc_cntr_init(
            &cntr,
            send,
            asrtc_default_allocator(),
            asrtc_default_error_cb(),
            &record_init_cb,
            &init_status,
            3 );
        CHECK_EQ( ASRTC_SUCCESS, st );

        // first tick sends the request and transitions to WAITING
        check_cntr_tick( &cntr );
        coll.data.pop_back();

        // 2 waiting ticks — not yet timed out
        check_cntr_tick( &cntr );
        check_cntr_tick( &cntr );
        CHECK( !asrtc_cntr_idle( &cntr ) );

        // 3rd waiting tick triggers timeout
        st = asrtc_cntr_tick( &cntr );
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

        check_cntr_tick( &cntr );
        coll.data.pop_back();

        check_cntr_tick( &cntr );
        check_cntr_tick( &cntr );
        CHECK( !asrtc_cntr_idle( &cntr ) );

        st = asrtc_cntr_tick( &cntr );
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

        check_cntr_tick( &cntr );
        coll.data.pop_back();

        check_cntr_tick( &cntr );
        check_cntr_tick( &cntr );
        CHECK( !asrtc_cntr_idle( &cntr ) );

        st = asrtc_cntr_tick( &cntr );
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

        check_cntr_tick( &cntr );
        coll.data.pop_back();

        check_cntr_tick( &cntr );
        check_cntr_tick( &cntr );
        CHECK( !asrtc_cntr_idle( &cntr ) );

        st = asrtc_cntr_tick( &cntr );
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

        check_cntr_tick( &cntr );
        coll.data.pop_back();

        check_cntr_tick( &cntr );
        check_cntr_tick( &cntr );
        CHECK( !asrtc_cntr_idle( &cntr ) );

        st = asrtc_cntr_tick( &cntr );
        CHECK_EQ( ASRTC_SUCCESS, st );
        CHECK_EQ( ASRTC_TIMEOUT_ERR, cb_status );
        CHECK( asrtc_cntr_idle( &cntr ) );
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_busy_err" )
{
        check_cntr_full_init( this );

        // Start an operation to put the controller into a non-idle state
        std::string       desc;
        enum asrtc_status st = asrtc_cntr_desc( &cntr, &cpy_desc_cb, (void*) &desc, 0 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        CHECK( !asrtc_cntr_idle( &cntr ) );

        // Any subsequent operation while busy must return BUSY_ERR
        st = asrtc_cntr_desc( &cntr, &cpy_desc_cb, (void*) &desc, 0 );
        CHECK_EQ( ASRTC_CNTR_BUSY_ERR, st );

        uint32_t x;
        st = asrtc_cntr_test_count( &cntr, &cpy_u32_cb, (void*) &x, 0 );
        CHECK_EQ( ASRTC_CNTR_BUSY_ERR, st );

        st = asrtc_cntr_test_info( &cntr, 0, &cpy_test_info_cb, (void*) &desc, 0 );
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

        enum asrtc_status st = asrtc_cntr_init(
            &cntr, send, asrtc_default_allocator(), ecb, &record_init_cb, &init_status, 0 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr );
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

// C-cov3 + C-cov4: wrong run_id in TEST_RESULT → ASRTC_TEST_ERROR in callback
TEST_CASE_FIXTURE( controller_ctx, "cntr_test_exec_wrong_run_id" )
{
        check_cntr_full_init( this );

        struct asrtc_result res = { 0 };
        enum asrtc_status   st  = asrtc_cntr_test_exec( &cntr, 42, result_cb, &res, 0 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr );  // sends TEST_START request
        coll.data.pop_back();

        // Send TEST_RESULT with wrong run_id (controller expects 0, send 99)
        uint8_t           buf[16];
        struct asrtl_span sp = { .b = buf, .e = buf + sizeof buf };
        asrtl_msg_rtoc_test_result( 99, ASRTL_TEST_SUCCESS, asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, buf, sp.b );

        CHECK_EQ( ASRTC_TEST_ERROR, res.res );
        CHECK_EQ( coll.data.empty(), true );
}

// C-cov5: recv while controller is IDLE → ASRTL_RECV_UNEXPECTED_ERR
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

// C-cov6: empty buffer — top-level header truncation
TEST_CASE_FIXTURE( controller_ctx, "cntr_recv_truncated_hdr" )
{
        enum asrtc_status st = asrtc_cntr_init(
            &cntr,
            send,
            asrtc_default_allocator(),
            asrtc_default_error_cb(),
            &record_init_cb,
            &init_status,
            0 );
        CHECK_EQ( ASRTC_SUCCESS, st );

        uint8_t           buf[1];
        enum asrtl_status rst =
            asrtc_cntr_recv( &cntr, (struct asrtl_span) { .b = buf, .e = buf } );
        CHECK_EQ( ASRTL_RECV_ERR, rst );

        // Drain: tick to send request then satisfy it
        check_cntr_tick( &cntr );
        coll.data.pop_back();
        uint8_t           vbuf[16];
        struct asrtl_span vsp = { .b = vbuf, .e = vbuf + sizeof vbuf };
        asrtl_msg_rtoc_proto_version( 0, 1, 0, asrtl_rec_span_to_span_cb, &vsp );
        check_recv_and_spin( &cntr, vbuf, vsp.b );
}

// C-cov6: truncated proto-version reply while in INIT/WAITING
TEST_CASE_FIXTURE( controller_ctx, "cntr_recv_truncated_init" )
{
        enum asrtc_status st = asrtc_cntr_init(
            &cntr,
            send,
            asrtc_default_allocator(),
            asrtc_default_error_cb(),
            &record_init_cb,
            &init_status,
            0 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr );
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
        check_recv_and_spin( &cntr, buf, sp.b );
}

// C-cov6: truncated test-count reply while in HNDL_TC/WAITING
TEST_CASE_FIXTURE( controller_ctx, "cntr_recv_truncated_test_count" )
{
        check_cntr_full_init( this );

        enum asrtc_status cb_st = ASRTC_SUCCESS;
        enum asrtc_status st    = asrtc_cntr_test_count( &cntr, &record_tc_cb, &cb_st, 0 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr );
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
        check_recv_and_spin( &cntr, buf, sp.b );
}

// C-cov6: truncated test-info reply while in HNDL_TI/WAITING
TEST_CASE_FIXTURE( controller_ctx, "cntr_recv_truncated_test_info" )
{
        check_cntr_full_init( this );

        struct test_info_result p  = { 0 };
        enum asrtc_status       st = asrtc_cntr_test_info( &cntr, 7, &cpy_test_info_cb, &p, 0 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr );
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
        check_recv_and_spin( &cntr, buf, sp.b );
}

//---------------------------------------------------------------------
// diag channel — controller side  (§2 of plan)

static inline enum asrtl_status call_diag_recv( struct asrtc_diag* d, uint8_t* b, uint8_t* e )
{
        return d->node.recv_cb( d->node.recv_ptr, (struct asrtl_span) { .b = b, .e = e } );
}

struct diag_ctx
{
        struct asrtl_node  head      = {};
        stub_allocator_ctx alloc_ctx = {};
        asrtc_allocator    alloc     = {};
        asrtc_diag         diag      = {};
        asrtl_sender       null_send = {};

        diag_ctx()
        {
                head.chid = ASRTL_CORE;
                alloc     = asrtc_stub_allocator( &alloc_ctx );
                REQUIRE_EQ( ASRTC_SUCCESS, asrtc_diag_init( &diag, &head, null_send, alloc ) );
        }
};

// C-INIT-1..3
TEST_CASE( "diag_init" )
{
        struct asrtl_node head = {};
        head.chid              = ASRTL_CORE;
        asrtl_sender null_send = {};

        // C-INIT-1: diag = NULL
        CHECK_EQ(
            ASRTC_CNTR_INIT_ERR,
            asrtc_diag_init( NULL, &head, null_send, asrtc_default_allocator() ) );

        // C-INIT-2: prev = NULL
        struct asrtc_diag diag = {};
        CHECK_EQ(
            ASRTC_CNTR_INIT_ERR,
            asrtc_diag_init( &diag, NULL, null_send, asrtc_default_allocator() ) );

        // C-INIT-3: valid
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

// C-RECV-1,3,4,8: happy-path recv
TEST_CASE_FIXTURE( diag_ctx, "diag_recv" )
{
        uint8_t  buf[64];
        uint8_t* p;

        // C-RECV-1: empty buffer → SUCCESS, no record queued
        CHECK_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, buf ) );
        CHECK_EQ( nullptr, diag.first_rec );

        // C-RECV-3: valid RECORD line=7, file="foo"
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 7 );
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

        // C-RECV-4: empty filename (5 bytes total) → file is empty string
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 42 );
        CHECK_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, p ) );
        {
                auto* rec = asrtc_diag_take_record( &diag );
                REQUIRE_NE( nullptr, rec );
                CHECK_EQ( 42u, rec->line );
                REQUIRE_NE( nullptr, rec->file );
                CHECK_EQ( 0u, strlen( rec->file ) );
                asrtc_diag_free_record( &diag.alloc, rec );
        }

        // C-RECV-8: two RECORDs → FIFO order
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 1 );
        *p++ = 'a';
        CHECK_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, p ) );
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 2 );
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

// C-RECV-2,5,6: error cases
TEST_CASE_FIXTURE( diag_ctx, "diag_recv_errors" )
{
        uint8_t buf[8];

        // C-RECV-2: msgid present but line is truncated (only 3 of 4 bytes)
        uint8_t* p = buf;
        *p++       = ASRTL_DIAG_MSG_RECORD;
        *p++       = 0x00;
        *p++       = 0x00;
        *p++       = 0x07;  // 3 bytes for line, need 4
        CHECK_EQ( ASRTL_RECV_ERR, call_diag_recv( &diag, buf, p ) );
        CHECK_EQ( nullptr, diag.first_rec );

        // C-RECV-5: unknown ID 0x00
        buf[0] = 0x00;
        CHECK_EQ( ASRTL_RECV_UNEXPECTED_ERR, call_diag_recv( &diag, buf, buf + 1 ) );
        CHECK_EQ( nullptr, diag.first_rec );

        // C-RECV-6: unknown ID 0xFF
        buf[0] = 0xFF;
        CHECK_EQ( ASRTL_RECV_UNEXPECTED_ERR, call_diag_recv( &diag, buf, buf + 1 ) );
        CHECK_EQ( nullptr, diag.first_rec );

        asrtc_diag_deinit( &diag );
}

// C-ALLOC-1,2: allocation failures during recv
TEST_CASE_FIXTURE( diag_ctx, "diag_recv_alloc_failure" )
{
        uint8_t  buf[8];
        uint8_t* p;

        // C-ALLOC-1: first alloc (rec struct) fails
        alloc_ctx.fail_at_call = 1;
        p                      = buf;
        *p++                   = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 10 );
        *p++ = 'x';
        CHECK_EQ( ASRTL_ALLOC_ERR, call_diag_recv( &diag, buf, p ) );
        CHECK_EQ( nullptr, diag.first_rec );
        CHECK_EQ( 0u, alloc_ctx.free_calls );

        // C-ALLOC-2: second alloc (file string) fails; rec is freed
        alloc_ctx.alloc_calls  = 0;
        alloc_ctx.free_calls   = 0;
        alloc_ctx.fail_at_call = 2;
        p                      = buf;
        *p++                   = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 20 );
        *p++ = 'y';
        CHECK_EQ( ASRTL_ALLOC_ERR, call_diag_recv( &diag, buf, p ) );
        CHECK_EQ( nullptr, diag.first_rec );
        CHECK_EQ( 1u, alloc_ctx.free_calls );

        alloc_ctx.fail_at_call = 0;
        asrtc_diag_deinit( &diag );
}

// C-TAKE-1..7
TEST_CASE_FIXTURE( diag_ctx, "diag_take_record" )
{
        uint8_t  buf[8];
        uint8_t* p;

        // C-TAKE-1: NULL diag
        CHECK_EQ( nullptr, asrtc_diag_take_record( NULL ) );

        // C-TAKE-2: empty queue
        CHECK_EQ( nullptr, asrtc_diag_take_record( &diag ) );

        // C-TAKE-3: one record → returned, queue empty after
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 99 );
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
                *p++ = 'a';
                REQUIRE_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, p ) );
        }
        for ( uint32_t i = 1; i <= 3; i++ ) {
                auto* rec = asrtc_diag_take_record( &diag );
                REQUIRE_NE( nullptr, rec );
                CHECK_EQ( i, rec->line );
                asrtc_diag_free_record( &diag.alloc, rec );
        }

        // C-TAKE-6: queue empty; take returns NULL
        CHECK_EQ( nullptr, asrtc_diag_take_record( &diag ) );

        // C-TAKE-7: take, insert, take → last_rec stays consistent
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 10 );
        *p++ = 'x';
        REQUIRE_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, p ) );
        auto* rec_a = asrtc_diag_take_record( &diag );
        REQUIRE_NE( nullptr, rec_a );
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 20 );
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
        asrtc_allocator    alloc     = asrtc_stub_allocator( &sctx );
        asrtl_sender       null_send = {};
        struct asrtc_diag  diag      = {};
        REQUIRE_EQ( ASRTC_SUCCESS, asrtc_diag_init( &diag, &head, null_send, alloc ) );

        uint8_t  buf[8];
        uint8_t* p = buf;
        *p++       = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 5 );
        *p++ = 'h';
        *p++ = 'i';
        REQUIRE_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, p ) );

        // C-FREE-1: record with non-NULL file → two free calls
        auto* rec = asrtc_diag_take_record( &diag );
        REQUIRE_NE( nullptr, rec );
        REQUIRE_NE( nullptr, rec->file );
        sctx.free_calls = 0;
        asrtc_diag_free_record( &alloc, rec );
        CHECK_EQ( 2u, sctx.free_calls );

        // C-FREE-2: record with file == NULL → one free call
        auto* rec2 =
            static_cast< asrtc_diag_record* >( asrtc_alloc( &alloc, sizeof( asrtc_diag_record ) ) );
        REQUIRE_NE( nullptr, rec2 );
        *rec2           = { .file = nullptr, .line = 0, .next = nullptr };
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

        // C-DEINIT-1: NULL → error
        CHECK_EQ( ASRTC_CNTR_INIT_ERR, asrtc_diag_deinit( NULL ) );

        // C-DEINIT-2: empty queue → success
        {
                struct asrtl_node head2      = {};
                head2.chid                   = ASRTL_CORE;
                asrtl_sender      null_send2 = {};
                struct asrtc_diag d2         = {};
                asrtc_diag_init( &d2, &head2, null_send2, asrtc_default_allocator() );
                CHECK_EQ( ASRTC_SUCCESS, asrtc_diag_deinit( &d2 ) );
        }

        // C-DEINIT-3: one record → freed
        {
                struct asrtl_node head3       = {};
                head3.chid                    = ASRTL_CORE;
                stub_allocator_ctx sctx3      = {};
                asrtc_allocator    a3         = asrtc_stub_allocator( &sctx3 );
                asrtl_sender       null_send3 = {};
                struct asrtc_diag  d3         = {};
                asrtc_diag_init( &d3, &head3, null_send3, a3 );
                p    = buf;
                *p++ = ASRTL_DIAG_MSG_RECORD;
                asrtl_add_u32( &p, 1 );
                *p++ = 'a';
                REQUIRE_EQ( ASRTL_SUCCESS, call_diag_recv( &d3, buf, p ) );
                CHECK_EQ( ASRTC_SUCCESS, asrtc_diag_deinit( &d3 ) );
                CHECK_EQ( nullptr, d3.first_rec );
                CHECK_EQ( 2u, sctx3.free_calls );  // file + rec
        }

        // C-DEINIT-4: three records → all freed
        {
                struct asrtl_node head4       = {};
                head4.chid                    = ASRTL_CORE;
                stub_allocator_ctx sctx4      = {};
                asrtc_allocator    a4         = asrtc_stub_allocator( &sctx4 );
                asrtl_sender       null_send4 = {};
                struct asrtc_diag  d4         = {};
                asrtc_diag_init( &d4, &head4, null_send4, a4 );
                for ( uint32_t i = 0; i < 3; i++ ) {
                        p    = buf;
                        *p++ = ASRTL_DIAG_MSG_RECORD;
                        asrtl_add_u32( &p, i );
                        *p++ = 'x';
                        REQUIRE_EQ( ASRTL_SUCCESS, call_diag_recv( &d4, buf, p ) );
                }
                CHECK_EQ( ASRTC_SUCCESS, asrtc_diag_deinit( &d4 ) );
                CHECK_EQ( nullptr, d4.first_rec );
                CHECK_EQ( 6u, sctx4.free_calls );  // 3 × (file + rec)
        }

        // C-DEINIT-5: partial take then deinit frees remaining
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 1 );
        *p++ = 'a';
        REQUIRE_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, p ) );
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 2 );
        *p++ = 'b';
        REQUIRE_EQ( ASRTL_SUCCESS, call_diag_recv( &diag, buf, p ) );
        auto* taken = asrtc_diag_take_record( &diag );
        REQUIRE_NE( nullptr, taken );
        asrtc_diag_free_record( &diag.alloc, taken );
        CHECK_EQ( ASRTC_SUCCESS, asrtc_diag_deinit( &diag ) );
        CHECK_EQ( nullptr, diag.first_rec );
}

// C-cov6: truncated exec messages while in HNDL_EXEC/WAITING
TEST_CASE_FIXTURE( controller_ctx, "cntr_recv_truncated_exec" )
{
        check_cntr_full_init( this );

        struct asrtc_result res = { 0 };
        enum asrtc_status   st  = asrtc_cntr_test_exec( &cntr, 1, result_cb, &res, 0 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        check_cntr_tick( &cntr );
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
        check_recv_and_spin( &cntr, buf, sp.b );
        CHECK_EQ( ASRTC_TEST_SUCCESS, res.res );
        CHECK_EQ( coll.data.empty(), true );
}
