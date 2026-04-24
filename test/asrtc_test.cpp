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
#include "../asrtc/collect.h"
#include "../asrtc/controller.h"
#include "../asrtc/default_allocator.h"
#include "../asrtc/default_error_cb.h"
#include "../asrtc/diag.h"
#include "../asrtc/param.h"
#include "../asrtl/collect_proto.h"
#include "../asrtl/core_proto.h"
#include "../asrtl/log.h"
#include "../asrtl/proto_version.h"
#include "../asrtl/util.h"
#include "../asrtlpp/flat_type_traits.hpp"
#include "../asrtr/collect.h"
#include "../asrtr/param.h"
#include "./collector.hpp"
#include "./stub_allocator.hpp"
#include "./util.h"

#include <doctest/doctest.h>

ASRTL_DEFINE_GPOS_LOG()

//---------------------------------------------------------------------
// lib

void check_recv( auto* c, struct asrtl_span msg )
{
        enum asrtl_status st = asrtl_chann_recv( &c->node, msg );
        CHECK_EQ( ASRTL_SUCCESS, st );
}

void check_tick( auto* c, uint32_t now )
{
        enum asrtl_status st = asrtl_chann_tick( &c->node, now );
        CHECK_EQ( ASRTL_SUCCESS, st );
}

void check_recv_and_spin( struct asrtc_controller* c, uint8_t* beg, uint8_t* end, uint32_t* now )
{
        check_recv( c, (struct asrtl_span) { .b = beg, .e = end } );
        int       i = 0;
        int const n = 1000;
        for ( ; i < n && !asrtc_cntr_idle( c ); i++ )
                check_tick( c, ( *now )++ );
        CHECK_NE( i, n );
}

struct controller_ctx
{
        struct asrtc_controller cntr = {};
        collector               coll;
        struct asrtl_sender     send        = {};
        uint8_t                 buffer[128] = {};
        struct asrtl_span       sp          = {};
        enum asrtl_status       init_status = {};
        uint32_t                t           = 1;

        controller_ctx()
        {
                sp = { buffer, buffer + sizeof buffer };
                setup_sender_collector( &send, &coll );
        }
        ~controller_ctx() { CHECK_EQ( coll.data.size(), 0 ); }
};

enum asrtl_status record_init_cb( void* ptr, enum asrtl_status s )
{
        enum asrtl_status* p = (enum asrtl_status*) ptr;
        *p                   = s;
        return s ? ASRTL_SUCCESS : ASRTL_SEND_ERR;  // TODO: incorrect error type
}

void check_cntr_full_init( controller_ctx* ctx )
{
        enum asrtl_status st = asrtc_cntr_init(
            &ctx->cntr, ctx->send, asrtc_default_allocator(), asrtc_default_error_cb() );
        CHECK_EQ( ASRTL_SUCCESS, st );
        st = asrtc_cntr_start( &ctx->cntr, &record_init_cb, &ctx->init_status, 1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
        check_tick( &ctx->cntr, ctx->t++ );

        assert_collected_core_hdr( ctx->coll.data.back(), 0x02, ASRTL_MSG_PROTO_VERSION );
        ctx->coll.data.pop_back();

        uint8_t           buffer[64];
        struct asrtl_span sp = {
            .b = buffer,
            .e = buffer + sizeof buffer,
        };

        asrtl_msg_rtoc_proto_version( 0, 1, 0, asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &ctx->cntr, buffer, sp.b, &ctx->t );
        CHECK_EQ( ASRTL_SUCCESS, ctx->init_status );
}


//---------------------------------------------------------------------
// tests

TEST_CASE_FIXTURE( controller_ctx, "cntr_init" )
{
        enum asrtl_status st;
        st = asrtc_cntr_init( NULL, send, asrtc_default_allocator(), asrtc_default_error_cb() );
        CHECK_EQ( ASRTL_INIT_ERR, st );

        st = asrtc_cntr_init( &cntr, send, asrtc_default_allocator(), asrtc_default_error_cb() );
        CHECK_EQ( ASRTL_SUCCESS, st );
        CHECK_EQ( ASRTL_CORE, cntr.node.chid );
        CHECK( asrtc_cntr_idle( &cntr ) );

        st = asrtc_cntr_start( &cntr, &record_init_cb, &init_status, 1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
        CHECK_EQ( ASRTC_CNTR_INIT, cntr.state );
        CHECK( !asrtc_cntr_idle( &cntr ) );

        check_tick( &cntr, t++ );

        assert_collected_core_hdr( coll.data.back(), 0x02, ASRTL_MSG_PROTO_VERSION );
        coll.data.pop_back();

        asrtl_msg_rtoc_proto_version( 0, 1, 0, asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, buffer, sp.b, &t );

        CHECK( asrtc_cntr_idle( &cntr ) );
        CHECK_EQ( ASRTL_SUCCESS, init_status );
}

enum asrtl_status cpy_desc_cb( void* ptr, enum asrtl_status s, char* desc )
{
        (void) s;
        std::string* p = (std::string*) ptr;
        *p             = desc;
        return ASRTL_SUCCESS;
}

struct test_info_result
{
        uint16_t    tid;
        std::string desc;
};

enum asrtl_status cpy_test_info_cb( void* ptr, enum asrtl_status s, uint16_t tid, char* desc )
{
        (void) s;
        struct test_info_result* r = (struct test_info_result*) ptr;
        r->tid                     = tid;
        r->desc                    = desc;
        return ASRTL_SUCCESS;
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_desc" )
{
        enum asrtl_status st;
        check_cntr_full_init( this );

        std::string desc;
        st = asrtc_cntr_desc( &cntr, &cpy_desc_cb, (void*) &desc, 1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
        check_tick( &cntr, t++ );

        assert_collected_core_hdr( coll.data.back(), 0x02, ASRTL_MSG_DESC );
        coll.data.pop_back();

        char const* msg = "wololo1";
        asrtl_msg_rtoc_desc( msg, strlen( msg ), asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, buffer, sp.b, &t );

        CHECK( desc != "" );
        CHECK( msg == desc );
}

enum asrtl_status cpy_u32_cb( void* ptr, enum asrtl_status s, uint16_t x )
{
        (void) s;
        uint32_t* p = (uint32_t*) ptr;
        *p          = x;
        return ASRTL_SUCCESS;
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_test_count" )
{
        enum asrtl_status st;
        check_cntr_full_init( this );

        uint32_t p = 0;
        st         = asrtc_cntr_test_count( &cntr, &cpy_u32_cb, (void*) &p, 1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
        check_tick( &cntr, t++ );

        assert_collected_core_hdr( coll.data.back(), 0x02, ASRTL_MSG_TEST_COUNT );
        coll.data.pop_back();

        asrtl_msg_rtoc_test_count( 42, asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, buffer, sp.b, &t );

        CHECK_EQ( 42, p );
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_test_info" )
{
        enum asrtl_status st;
        check_cntr_full_init( this );

        struct test_info_result p = { 0 };
        st = asrtc_cntr_test_info( &cntr, 42, &cpy_test_info_cb, (void*) &p, 1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
        check_tick( &cntr, t++ );

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
        enum asrtl_status       st =
            asrtc_cntr_test_info( &cntr, 42, &cpy_test_info_cb, (void*) &p, 1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
        check_tick( &cntr, t++ );
        coll.data.pop_back();

        // Reply with a different tid (99 instead of 42)
        uint8_t           buf[64];
        struct asrtl_span sp   = { .b = buf, .e = buf + sizeof buf };
        char const*       desc = "barbaz";
        asrtl_msg_rtoc_test_info( 99, desc, strlen( desc ), asrtl_rec_span_to_span_cb, &sp );
        enum asrtl_status rst =
            asrtl_chann_recv( &cntr.node, (struct asrtl_span) { .b = buf, .e = sp.b } );
        CHECK_EQ( ASRTL_RECV_UNEXPECTED_ERR, rst );

        CHECK( p.desc.empty() );
}

enum asrtl_status result_cb( void* ptr, enum asrtl_status s, struct asrtc_result* res )
{
        (void) s;
        struct asrtc_result* r1 = (struct asrtc_result*) ptr;
        *r1                     = *res;
        return ASRTL_SUCCESS;
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
        enum asrtl_status st =
            asrtc_cntr_init( &cntr, send, failing_allocator(), asrtc_default_error_cb() );
        CHECK_EQ( ASRTL_SUCCESS, st );
        st = asrtc_cntr_start( &cntr, &record_init_cb, &init_status, 1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
        check_tick( &cntr, t++ );
        coll.data.pop_back();  // discard PROTO_VERSION request

        // Simulate receiving a proto-version reply to advance to IDLE
        uint8_t           buf[64];
        struct asrtl_span sp = { .b = buf, .e = buf + sizeof buf };
        asrtl_msg_rtoc_proto_version( 0, 1, 0, asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, buf, sp.b, &t );
        CHECK_EQ( ASRTL_SUCCESS, init_status );

        std::string desc;
        st = asrtc_cntr_desc( &cntr, &cpy_desc_cb, (void*) &desc, 1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
        check_tick( &cntr, t++ );
        coll.data.pop_back();  // discard DESC request

        // Send a DESC reply — alloc will return NULL, recv must return an error
        sp              = (struct asrtl_span) { .b = buf, .e = buf + sizeof buf };
        char const* msg = "hello";
        asrtl_msg_rtoc_desc( msg, strlen( msg ), asrtl_rec_span_to_span_cb, &sp );
        enum asrtl_status rst =
            asrtl_chann_recv( &cntr.node, (struct asrtl_span) { .b = buf, .e = sp.b } );
        CHECK_NE( ASRTL_SUCCESS, rst );
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_run_test" )
{
        enum asrtl_status st;
        check_cntr_full_init( this );

        struct asrtc_result res;
        st = asrtc_cntr_test_exec( &cntr, 42, result_cb, &res, 1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
        check_tick( &cntr, t++ );

        assert_collected_core_hdr( coll.data.back(), 0x08, ASRTL_MSG_TEST_START );
        assert_u16( 42, coll.data.back().data.data() + 2 );
        assert_u32( 0, coll.data.back().data.data() + 4 );
        coll.data.pop_back();

        for ( int i = 0; i < 4; i++ )
                check_tick( &cntr, t++ );

        asrtl_msg_rtoc_test_start( 42, 0, asrtl_rec_span_to_span_cb, &sp );
        check_recv( &cntr, (struct asrtl_span) { .b = buffer, .e = sp.b } );
        for ( int i = 0; i < 4; i++ )
                check_tick( &cntr, t++ );

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
        enum asrtl_status st =
            asrtc_cntr_init( &cntr, send, asrtc_default_allocator(), asrtc_default_error_cb() );
        CHECK_EQ( ASRTL_SUCCESS, st );
        st = asrtc_cntr_start( &cntr, &record_init_cb, &init_status, 1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
        check_tick( &cntr, t++ );
        coll.data.pop_back();

        // reply with a mismatched major version
        uint8_t           buf[64];
        struct asrtl_span sp = { .b = buf, .e = buf + sizeof buf };
        asrtl_msg_rtoc_proto_version( ASRTL_PROTO_MAJOR + 1, 0, 0, asrtl_rec_span_to_span_cb, &sp );
        check_recv( &cntr, (struct asrtl_span) { .b = buf, .e = sp.b } );

        enum asrtl_status st2 = asrtl_chann_tick( &cntr.node, t++ );
        CHECK_EQ( ASRTL_VERSION_ERR, init_status );
        CHECK( asrtc_cntr_idle( &cntr ) );
}

// ---------------------------------------------------------------------------
// timeout tests

static enum asrtl_status record_status_cb( void* ptr, enum asrtl_status s )
{
        enum asrtl_status* p = (enum asrtl_status*) ptr;
        *p                   = s;
        return ASRTL_SUCCESS;
}

static enum asrtl_status record_tc_cb( void* ptr, enum asrtl_status s, uint16_t count )
{
        (void) count;
        return record_status_cb( ptr, s );
}

static enum asrtl_status record_desc_cb( void* ptr, enum asrtl_status s, char* desc )
{
        (void) desc;
        return record_status_cb( ptr, s );
}

static enum asrtl_status record_test_info_cb(
    void*             ptr,
    enum asrtl_status s,
    uint16_t          tid,
    char*             desc )
{
        (void) tid;
        (void) desc;
        return record_status_cb( ptr, s );
}

static enum asrtl_status record_result_cb(
    void*                ptr,
    enum asrtl_status    s,
    struct asrtc_result* res )
{
        (void) res;
        return record_status_cb( ptr, s );
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_timeout_init" )
{
        enum asrtl_status st =
            asrtc_cntr_init( &cntr, send, asrtc_default_allocator(), asrtc_default_error_cb() );
        CHECK_EQ( ASRTL_SUCCESS, st );
        st = asrtc_cntr_start( &cntr, &record_init_cb, &init_status, 3 );
        CHECK_EQ( ASRTL_SUCCESS, st );

        // now=0: sends request, enters WAITING, deadline = 0+3 = 3
        check_tick( &cntr, 0 );
        coll.data.pop_back();

        // now=1,2: still waiting
        check_tick( &cntr, 1 );
        check_tick( &cntr, 2 );
        CHECK( !asrtc_cntr_idle( &cntr ) );

        // now=3: deadline reached → timeout
        enum asrtl_status st2 = asrtl_chann_tick( &cntr.node, 3 );
        CHECK_EQ( ASRTL_TIMEOUT_ERR, init_status );
        CHECK( asrtc_cntr_idle( &cntr ) );
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_timeout_test_count" )
{
        check_cntr_full_init( this );

        enum asrtl_status cb_status = ASRTL_SUCCESS;
        enum asrtl_status st        = asrtc_cntr_test_count( &cntr, &record_tc_cb, &cb_status, 3 );
        CHECK_EQ( ASRTL_SUCCESS, st );

        check_tick( &cntr, 0 );
        coll.data.pop_back();

        check_tick( &cntr, 1 );
        check_tick( &cntr, 2 );
        CHECK( !asrtc_cntr_idle( &cntr ) );

        check_tick( &cntr, 3 );
        CHECK_EQ( ASRTL_TIMEOUT_ERR, cb_status );
        CHECK( asrtc_cntr_idle( &cntr ) );
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_timeout_desc" )
{
        check_cntr_full_init( this );

        enum asrtl_status cb_status = ASRTL_SUCCESS;
        enum asrtl_status st        = asrtc_cntr_desc( &cntr, &record_desc_cb, &cb_status, 3 );
        CHECK_EQ( ASRTL_SUCCESS, st );

        check_tick( &cntr, 0 );
        coll.data.pop_back();

        check_tick( &cntr, 1 );
        check_tick( &cntr, 2 );
        CHECK( !asrtc_cntr_idle( &cntr ) );

        check_tick( &cntr, 3 );
        CHECK_EQ( ASRTL_TIMEOUT_ERR, cb_status );
        CHECK( asrtc_cntr_idle( &cntr ) );
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_timeout_test_info" )
{
        check_cntr_full_init( this );

        enum asrtl_status cb_status = ASRTL_SUCCESS;
        enum asrtl_status st =
            asrtc_cntr_test_info( &cntr, 0, &record_test_info_cb, &cb_status, 3 );
        CHECK_EQ( ASRTL_SUCCESS, st );

        check_tick( &cntr, 0 );
        coll.data.pop_back();

        check_tick( &cntr, 1 );
        check_tick( &cntr, 2 );
        CHECK( !asrtc_cntr_idle( &cntr ) );

        check_tick( &cntr, 3 );
        CHECK_EQ( ASRTL_TIMEOUT_ERR, cb_status );
        CHECK( asrtc_cntr_idle( &cntr ) );
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_timeout_exec" )
{
        check_cntr_full_init( this );

        enum asrtl_status cb_status = ASRTL_SUCCESS;
        enum asrtl_status st = asrtc_cntr_test_exec( &cntr, 0, &record_result_cb, &cb_status, 3 );
        CHECK_EQ( ASRTL_SUCCESS, st );

        // now=0: STAGE_INIT → sends, enters WAITING, deadline = 0+3 = 3
        check_tick( &cntr, 0 );
        coll.data.pop_back();

        // now=1,2: still waiting
        check_tick( &cntr, 1 );
        check_tick( &cntr, 2 );
        CHECK( !asrtc_cntr_idle( &cntr ) );

        // now=3: deadline reached → timeout
        check_tick( &cntr, 3 );
        CHECK_EQ( ASRTL_TIMEOUT_ERR, cb_status );
        CHECK( asrtc_cntr_idle( &cntr ) );
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_busy_err" )
{
        check_cntr_full_init( this );

        // Start an operation to put the controller into a non-idle state
        std::string       desc;
        enum asrtl_status st = asrtc_cntr_desc( &cntr, &cpy_desc_cb, (void*) &desc, 1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
        CHECK( !asrtc_cntr_idle( &cntr ) );

        // Any subsequent operation while busy must return BUSY_ERR
        st = asrtc_cntr_desc( &cntr, &cpy_desc_cb, (void*) &desc, 1000 );
        CHECK_EQ( ASRTL_BUSY_ERR, st );

        uint32_t x;
        st = asrtc_cntr_test_count( &cntr, &cpy_u32_cb, (void*) &x, 1000 );
        CHECK_EQ( ASRTL_BUSY_ERR, st );

        st = asrtc_cntr_test_info( &cntr, 0, &cpy_test_info_cb, (void*) &desc, 1000 );
        CHECK_EQ( ASRTL_BUSY_ERR, st );
}

struct error_result
{
        enum asrtl_source src;
        enum asrtl_ecode  ecode;
};

static enum asrtl_status record_error_cb( void* ptr, enum asrtl_source src, enum asrtl_ecode ecode )
{
        struct error_result* r = (struct error_result*) ptr;
        r->src                 = src;
        r->ecode               = ecode;
        return ASRTL_SUCCESS;
}

TEST_CASE_FIXTURE( controller_ctx, "cntr_recv_error" )
{
        // Init with a custom error callback that records what it receives
        struct error_result   err = {};
        struct asrtc_error_cb ecb = { .ptr = &err, .cb = &record_error_cb };

        enum asrtl_status st = asrtc_cntr_init( &cntr, send, asrtc_default_allocator(), ecb );
        CHECK_EQ( ASRTL_SUCCESS, st );
        st = asrtc_cntr_start( &cntr, &record_init_cb, &init_status, 1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
        check_tick( &cntr, t++ );
        coll.data.pop_back();

        // Send an error message while the controller is waiting for a response
        uint8_t           buf[16];
        struct asrtl_span sp = { .b = buf, .e = buf + sizeof buf };
        asrtl_msg_rtoc_error( 42, asrtl_rec_span_to_span_cb, &sp );
        check_recv( &cntr, (struct asrtl_span) { .b = buf, .e = sp.b } );
        CHECK_EQ( ASRTL_REACTOR, err.src );
        CHECK_EQ( 42, err.ecode );
}

// wrong run_id in TEST_RESULT → ASRTC_TEST_ERROR in callback
TEST_CASE_FIXTURE( controller_ctx, "cntr_test_exec_wrong_run_id" )
{
        check_cntr_full_init( this );

        struct asrtc_result res = { 0 };
        enum asrtl_status   st  = asrtc_cntr_test_exec( &cntr, 42, result_cb, &res, 1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
        check_tick( &cntr, t++ );  // sends TEST_START request
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
            asrtl_chann_recv( &cntr.node, (struct asrtl_span) { .b = buf, .e = sp.b } );
        CHECK_EQ( ASRTL_RECV_UNEXPECTED_ERR, rst );
        CHECK( asrtc_cntr_idle( &cntr ) );
}

// empty buffer — top-level header truncation
TEST_CASE_FIXTURE( controller_ctx, "cntr_recv_truncated_hdr" )
{
        enum asrtl_status st =
            asrtc_cntr_init( &cntr, send, asrtc_default_allocator(), asrtc_default_error_cb() );
        CHECK_EQ( ASRTL_SUCCESS, st );
        st = asrtc_cntr_start( &cntr, &record_init_cb, &init_status, 1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );

        uint8_t           buf[1];
        enum asrtl_status rst =
            asrtl_chann_recv( &cntr.node, (struct asrtl_span) { .b = buf, .e = buf } );
        CHECK_EQ( ASRTL_RECV_ERR, rst );

        // Drain: tick to send request then satisfy it
        check_tick( &cntr, t++ );
        coll.data.pop_back();
        uint8_t           vbuf[16];
        struct asrtl_span vsp = { .b = vbuf, .e = vbuf + sizeof vbuf };
        asrtl_msg_rtoc_proto_version( 0, 1, 0, asrtl_rec_span_to_span_cb, &vsp );
        check_recv_and_spin( &cntr, vbuf, vsp.b, &t );
}

// truncated proto-version reply while in INIT/WAITING
TEST_CASE_FIXTURE( controller_ctx, "cntr_recv_truncated_init" )
{
        enum asrtl_status st =
            asrtc_cntr_init( &cntr, send, asrtc_default_allocator(), asrtc_default_error_cb() );
        CHECK_EQ( ASRTL_SUCCESS, st );
        st = asrtc_cntr_start( &cntr, &record_init_cb, &init_status, 1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
        check_tick( &cntr, t++ );
        coll.data.pop_back();

        // ID + only major(2) — missing minor(2) + patch(2) = too short
        uint8_t           buf[16];
        struct asrtl_span sp = { .b = buf, .e = buf + sizeof buf };
        asrtl_add_u16( &sp.b, ASRTL_MSG_PROTO_VERSION );
        asrtl_add_u16( &sp.b, 0 );  // major only, 4 bytes total
        enum asrtl_status rst =
            asrtl_chann_recv( &cntr.node, (struct asrtl_span) { .b = buf, .e = sp.b } );
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

        enum asrtl_status cb_st = ASRTL_SUCCESS;
        enum asrtl_status st    = asrtc_cntr_test_count( &cntr, &record_tc_cb, &cb_st, 1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
        check_tick( &cntr, t++ );
        coll.data.pop_back();

        // Just the message ID, no u16 count
        uint8_t           buf[16];
        struct asrtl_span sp = { .b = buf, .e = buf + sizeof buf };
        asrtl_add_u16( &sp.b, ASRTL_MSG_TEST_COUNT );
        enum asrtl_status rst =
            asrtl_chann_recv( &cntr.node, (struct asrtl_span) { .b = buf, .e = sp.b } );
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
        enum asrtl_status       st = asrtc_cntr_test_info( &cntr, 7, &cpy_test_info_cb, &p, 1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
        check_tick( &cntr, t++ );
        coll.data.pop_back();

        // Just the message ID, no u16 tid
        uint8_t           buf[16];
        struct asrtl_span sp = { .b = buf, .e = buf + sizeof buf };
        asrtl_add_u16( &sp.b, ASRTL_MSG_TEST_INFO );
        enum asrtl_status rst =
            asrtl_chann_recv( &cntr.node, (struct asrtl_span) { .b = buf, .e = sp.b } );
        CHECK_EQ( ASRTL_RECV_ERR, rst );

        // Satisfy properly to clean up
        char const* desc = "x";
        sp               = (struct asrtl_span) { .b = buf, .e = buf + sizeof buf };
        asrtl_msg_rtoc_test_info( 7, desc, strlen( desc ), asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &cntr, buf, sp.b, &t );
}

//---------------------------------------------------------------------
// diag channel — controller side


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
                REQUIRE_EQ( ASRTL_SUCCESS, asrtc_diag_init( &diag, &head, null_send, alloc ) );
        }
};

TEST_CASE( "diag_init" )
{
        struct asrtl_node head = {};
        head.chid              = ASRTL_CORE;
        asrtl_sender null_send = {};

        // diag = NULL
        CHECK_EQ(
            ASRTL_INIT_ERR, asrtc_diag_init( NULL, &head, null_send, asrtc_default_allocator() ) );

        // prev = NULL
        struct asrtc_diag diag = {};
        CHECK_EQ(
            ASRTL_INIT_ERR, asrtc_diag_init( &diag, NULL, null_send, asrtc_default_allocator() ) );

        // valid
        CHECK_EQ(
            ASRTL_SUCCESS, asrtc_diag_init( &diag, &head, null_send, asrtc_default_allocator() ) );
        CHECK_EQ( ASRTL_DIAG, diag.node.chid );
        CHECK_NE( nullptr, (void*) (uintptr_t) diag.node.e_cb_ptr );
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

        check_recv( &diag, (struct asrtl_span) { .b = buf, .e = buf } );
        CHECK_EQ( nullptr, diag.first_rec );

        // valid RECORD line=7, file="foo"
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 7 );
        *p++ = 3;  // file_len
        *p++ = 'f';
        *p++ = 'o';
        *p++ = 'o';
        check_recv( &diag, (struct asrtl_span) { .b = buf, .e = p } );
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
        check_recv( &diag, (struct asrtl_span) { .b = buf, .e = p } );
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
        check_recv( &diag, (struct asrtl_span) { .b = buf, .e = p } );
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 2 );
        *p++ = 1;  // file_len
        *p++ = 'b';
        check_recv( &diag, (struct asrtl_span) { .b = buf, .e = p } );
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
        CHECK_EQ(
            ASRTL_RECV_ERR,
            asrtl_chann_recv( &diag.node, (struct asrtl_span) { .b = buf, .e = p } ) );
        CHECK_EQ( nullptr, diag.first_rec );

        // unknown ID 0x00
        buf[0] = 0x00;
        CHECK_EQ(
            ASRTL_RECV_UNEXPECTED_ERR,
            asrtl_chann_recv( &diag.node, (struct asrtl_span) { .b = buf, .e = buf + 1 } ) );
        CHECK_EQ( nullptr, diag.first_rec );

        // unknown ID 0xFF
        buf[0] = 0xFF;
        CHECK_EQ(
            ASRTL_RECV_UNEXPECTED_ERR,
            asrtl_chann_recv( &diag.node, (struct asrtl_span) { .b = buf, .e = buf + 1 } ) );
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
        CHECK_EQ(
            ASRTL_ALLOC_ERR,
            asrtl_chann_recv( &diag.node, (struct asrtl_span) { .b = buf, .e = p } ) );
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
        CHECK_EQ(
            ASRTL_ALLOC_ERR,
            asrtl_chann_recv( &diag.node, (struct asrtl_span) { .b = buf, .e = p } ) );
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
        CHECK_EQ(
            ASRTL_SUCCESS,
            asrtl_chann_recv( &diag.node, (struct asrtl_span) { .b = buf, .e = p } ) );
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
                CHECK_EQ(
                    ASRTL_SUCCESS,
                    asrtl_chann_recv( &diag.node, (struct asrtl_span) { .b = buf, .e = p } ) );
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
        check_recv( &diag, (struct asrtl_span) { .b = buf, .e = p } );
        auto* rec_a = asrtc_diag_take_record( &diag );
        REQUIRE_NE( nullptr, rec_a );
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 20 );
        *p++ = 1;  // file_len
        *p++ = 'y';
        check_recv( &diag, (struct asrtl_span) { .b = buf, .e = p } );
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
        REQUIRE_EQ( ASRTL_SUCCESS, asrtc_diag_init( &diag, &head, null_send, alloc ) );

        uint8_t  buf[16];
        uint8_t* p = buf;
        *p++       = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 5 );
        *p++ = 2;  // file_len
        *p++ = 'h';
        *p++ = 'i';
        check_recv( &diag, (struct asrtl_span) { .b = buf, .e = p } );

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
        CHECK_EQ( ASRTL_INIT_ERR, asrtc_diag_deinit( NULL ) );

        // empty queue → success
        {
                struct asrtl_node head2      = {};
                head2.chid                   = ASRTL_CORE;
                asrtl_sender      null_send2 = {};
                struct asrtc_diag d2         = {};
                asrtc_diag_init( &d2, &head2, null_send2, asrtc_default_allocator() );
                CHECK_EQ( ASRTL_SUCCESS, asrtc_diag_deinit( &d2 ) );
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
                check_recv( &d3, (struct asrtl_span) { .b = buf, .e = p } );
                CHECK_EQ( ASRTL_SUCCESS, asrtc_diag_deinit( &d3 ) );
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
                        check_recv( &d4, (struct asrtl_span) { .b = buf, .e = p } );
                }
                CHECK_EQ( ASRTL_SUCCESS, asrtc_diag_deinit( &d4 ) );
                CHECK_EQ( nullptr, d4.first_rec );
                CHECK_EQ( 6u, sctx4.free_calls );  // 3 × (file + rec)
        }

        // partial take then deinit frees remaining
        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 1 );
        *p++ = 1;  // file_len
        *p++ = 'a';
        check_recv( &diag, (struct asrtl_span) { .b = buf, .e = p } );

        p    = buf;
        *p++ = ASRTL_DIAG_MSG_RECORD;
        asrtl_add_u32( &p, 2 );
        *p++ = 1;  // file_len
        *p++ = 'b';
        check_recv( &diag, (struct asrtl_span) { .b = buf, .e = p } );
        auto* taken = asrtc_diag_take_record( &diag );
        REQUIRE_NE( nullptr, taken );
        asrtc_diag_free_record( &diag.alloc, taken );
        CHECK_EQ( ASRTL_SUCCESS, asrtc_diag_deinit( &diag ) );
        CHECK_EQ( nullptr, diag.first_rec );
}

// truncated exec messages while in HNDL_EXEC/WAITING
TEST_CASE_FIXTURE( controller_ctx, "cntr_recv_truncated_exec" )
{
        check_cntr_full_init( this );

        struct asrtc_result res = { 0 };
        enum asrtl_status   st  = asrtc_cntr_test_exec( &cntr, 1, result_cb, &res, 1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
        check_tick( &cntr, t++ );
        coll.data.pop_back();

        uint8_t           buf[16];
        struct asrtl_span sp;

        // Truncated TEST_RESULT: ID + run_id(4) only — missing res(2)
        sp = (struct asrtl_span) { .b = buf, .e = buf + sizeof buf };
        asrtl_add_u16( &sp.b, ASRTL_MSG_TEST_RESULT );
        asrtl_add_u32( &sp.b, 0 );  // run_id, but no res u16
        enum asrtl_status rst =
            asrtl_chann_recv( &cntr.node, (struct asrtl_span) { .b = buf, .e = sp.b } );
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

        check_recv( &diag, (struct asrtl_span) { .b = buf, .e = sp.b } );

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

        check_recv( &diag, (struct asrtl_span) { .b = buf, .e = sp.b } );

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
        asrtl_flat_tree_append_cont( &tree, 0, 1, NULL, ASRTL_FLAT_CTYPE_OBJECT );
        asrtl_flat_tree_append_scalar(
            &tree, 1, 2, "alpha", ASRTL_FLAT_STYPE_U32, { .u32_val = 10 } );
        asrtl_flat_tree_append_scalar(
            &tree, 1, 3, "beta", ASRTL_FLAT_STYPE_STR, { .str_val = "hi" } );
        asrtl_flat_tree_append_scalar(
            &tree, 1, 4, "gamma", ASRTL_FLAT_STYPE_BOOL, { .bool_val = 1 } );
        return tree;
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
static uint8_t* build_query( uint8_t* buf, asrt::flat_id node_id )
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
                REQUIRE_EQ( ASRTL_SUCCESS, asrtc_param_server_init( &param, &head, sendr, alloc ) );
        }
        ~param_ctx() { asrtc_param_server_deinit( &param ); }
};

TEST_CASE( "asrtc_param_server_init" )
{
        struct asrtl_node head           = {};
        head.chid                        = ASRTL_CORE;
        asrtl_sender              null_s = {};
        struct asrtc_param_server param2 = {};

        CHECK_EQ(
            ASRTL_INIT_ERR,
            asrtc_param_server_init( NULL, &head, null_s, asrtl_default_allocator() ) );
        CHECK_EQ(
            ASRTL_INIT_ERR,
            asrtc_param_server_init( &param2, NULL, null_s, asrtl_default_allocator() ) );

        CHECK_EQ(
            ASRTL_SUCCESS,
            asrtc_param_server_init( &param2, &head, null_s, asrtl_default_allocator() ) );
        CHECK_EQ( ASRTL_PARA, param2.node.chid );
        CHECK_NE( nullptr, (void*) (uintptr_t) param2.node.e_cb_ptr );
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
        CHECK_EQ( ASRTL_PARA, msg.id );
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
        check_recv( &param, (struct asrtl_span) { .b = buf, .e = build_ready_ack( buf, 256u ) } );
        CHECK_EQ( ASRTL_SUCCESS, asrtl_chann_tick( &param.node, t++ ) );
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
        check_recv( &param, (struct asrtl_span) { .b = buf, .e = build_ready_ack( buf, 256u ) } );
        // pending == PENDING_READY_ACK; tick not called yet
        CHECK_EQ(
            ASRTL_RECV_ERR,
            asrtl_chann_recv(
                &param.node,
                (struct asrtl_span) { .b = buf, .e = build_ready_ack( buf, 512u ) } ) );
        // Consume the first pending so the fixture destructor stays clean
        check_tick( &param, t++ );
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
        check_recv( &param, (struct asrtl_span) { .b = buf, .e = build_ready_ack( buf, 256u ) } );
        check_tick( &param, t++ );

        // First query stores pending
        check_recv( &param, (struct asrtl_span) { .b = buf, .e = build_query( buf, 2u ) } );
        // pending == PENDING_QUERY; tick not called yet
        CHECK_EQ(
            ASRTL_RECV_ERR,
            asrtl_chann_recv(
                &param.node, (struct asrtl_span) { .b = buf, .e = build_query( buf, 3u ) } ) );
        check_tick( &param, t++ );  // second tick to consume the second pending if it were
                                    // erroneously accepted
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_ctx, "asrtc_param_server_query_before_ack_returns_error" )
{
        struct asrtl_flat_tree tree = make_param_tree();
        asrtc_param_server_set_tree( &param, &tree );

        uint8_t buf[8];
        CHECK_EQ(
            ASRTL_RECV_ERR,
            asrtl_chann_recv(
                &param.node, (struct asrtl_span) { .b = buf, .e = build_query( buf, 2u ) } ) );
        CHECK( coll.data.empty() );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_ctx, "asrtc_param_server_query_produces_response" )
{
        struct asrtl_flat_tree tree = make_param_tree();
        asrtc_param_server_set_tree( &param, &tree );

        uint8_t buf[8];
        // Handshake
        check_recv( &param, (struct asrtl_span) { .b = buf, .e = build_ready_ack( buf, 256u ) } );
        check_tick( &param, t++ );

        // Query node 2 ("alpha", u32=10)
        check_recv( &param, (struct asrtl_span) { .b = buf, .e = build_query( buf, 2u ) } );
        check_tick( &param, t++ );
        REQUIRE_EQ( 1u, coll.data.size() );

        auto& msg = coll.data.front();
        CHECK_EQ( ASRTL_PARA, msg.id );
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
        check_recv( &param, (struct asrtl_span) { .b = buf, .e = build_ready_ack( buf, 20u ) } );
        check_tick( &param, t++ );

        // First query: only node 2 fits → next_sibling_id points to node 3
        check_recv( &param, (struct asrtl_span) { .b = buf, .e = build_query( buf, 2u ) } );
        check_tick( &param, t++ );
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
        check_recv( &param, (struct asrtl_span) { .b = buf, .e = build_ready_ack( buf, 11u ) } );
        check_tick( &param, t++ );
        check_recv( &param, (struct asrtl_span) { .b = buf, .e = build_query( buf, 2u ) } );
        check_tick( &param, t++ );

        REQUIRE_EQ( 1u, coll.data.size() );
        auto& msg = coll.data.front();
        CHECK_EQ( ASRTL_PARA, msg.id );
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
        check_recv( &param, (struct asrtl_span) { .b = buf, .e = build_ready_ack( buf, 64u ) } );
        check_tick( &param, t++ );
        CHECK_NE( nullptr, param.enc_buff );
        CHECK_EQ( 1u, alloc_ctx.alloc_calls );
        CHECK_EQ( 0u, alloc_ctx.free_calls );

        asrtc_param_server_deinit( &param );
        CHECK_EQ( nullptr, param.enc_buff );
        CHECK_EQ( 1u, alloc_ctx.free_calls );

        // Prevent double-free in fixture destructor
        alloc_ctx.free_calls = 0;
}

template < typename Server, typename Client >
struct server_client_base
{
        Server   server = {};
        Client   client = {};
        uint32_t t      = 1;

        static enum asrtl_status srv_to_cli(
            void* ptr,
            asrtl_chann_id /*id*/,
            struct asrtl_rec_span* buff,
            asrtl_send_done_cb     done_cb,
            void*                  done_ptr )
        {
                auto*             ctx = (server_client_base*) ptr;
                uint8_t           flat[512];
                struct asrtl_span sp = { .b = flat, .e = flat + sizeof flat };
                asrtl_rec_span_to_span( &sp, buff );
                struct asrtl_span msg = { .b = flat, .e = sp.b };
                auto              st  = asrtl_chann_recv( &ctx->client.node, msg );
                if ( done_cb )
                        done_cb( done_ptr, st );
                return st;
        }

        static enum asrtl_status cli_to_srv(
            void* ptr,
            asrtl_chann_id /*id*/,
            struct asrtl_rec_span* buff,
            asrtl_send_done_cb     done_cb,
            void*                  done_ptr )
        {
                auto*             ctx = (server_client_base*) ptr;
                uint8_t           flat[512];
                struct asrtl_span sp = { .b = flat, .e = flat + sizeof flat };
                asrtl_rec_span_to_span( &sp, buff );
                struct asrtl_span msg = { .b = flat, .e = sp.b };
                auto              st  = asrtl_chann_recv( &ctx->server.node, msg );
                if ( done_cb )
                        done_cb( done_ptr, st );
                return st;
        }

        // Tick both sides up to N times until neither has pending work
        void spin( int max_iter = 100 )
        {
                for ( int i = 0; i < max_iter; i++ ) {
                        asrtl_chann_tick( &server.node, t++ );
                        asrtl_chann_tick( &client.node, t++ );
                }
        }
};
using param_base = server_client_base< asrtc_param_server, asrtr_param_client >;

struct param_loopback_ctx : param_base
{
        // Both sides share one head each (CORE placeholder + PARAM node)
        struct asrtl_node         srv_head            = {};
        struct asrtl_node         cli_head            = {};
        stub_allocator_ctx        alloc_ctx           = {};
        asrtl_allocator           alloc               = {};
        static constexpr uint32_t CLI_BUF_SZ          = 256;
        uint8_t                   cli_buf[CLI_BUF_SZ] = {};

        // Cross-wired senders: server sends → client recv, client sends → server recv
        asrtl_sender srv_sendr = {};
        asrtl_sender cli_sendr = {};

        // Response callback state
        struct received_node
        {
                asrt::flat_id    id;
                std::string      key;
                asrtl_flat_value value;
                asrt::flat_id    next_sibling;
        };
        std::vector< received_node > received;
        int                          error_called = 0;
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


        param_loopback_ctx()
        {
                srv_head.chid = ASRTL_CORE;
                cli_head.chid = ASRTL_CORE;
                alloc         = asrtl_stub_allocator( &alloc_ctx );
                srv_sendr     = asrtl_sender{ .ptr = this, .cb = srv_to_cli };
                cli_sendr     = asrtl_sender{ .ptr = this, .cb = cli_to_srv };
                REQUIRE_EQ(
                    ASRTL_SUCCESS,
                    asrtc_param_server_init( &server, &srv_head, srv_sendr, alloc ) );
                struct asrtl_span mb = { .b = cli_buf, .e = cli_buf + CLI_BUF_SZ };
                REQUIRE_EQ(
                    ASRTL_SUCCESS,
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
        asrtl_flat_tree_append_cont( &tree, 0, 1, NULL, ASRTL_FLAT_CTYPE_OBJECT );
        asrtl_flat_tree_append_cont( &tree, 1, 2, "sub", ASRTL_FLAT_CTYPE_OBJECT );
        asrtl_flat_tree_append_scalar( &tree, 1, 3, "b", ASRTL_FLAT_STYPE_BOOL, { .bool_val = 1 } );
        asrtl_flat_tree_append_scalar( &tree, 2, 4, "x", ASRTL_FLAT_STYPE_U32, { .u32_val = 100 } );
        asrtl_flat_tree_append_scalar(
            &tree, 2, 5, "y", ASRTL_FLAT_STYPE_STR, { .str_val = "hello" } );

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
            ASRTL_SUCCESS, asrtr_param_client_fetch_any( &query, &client, 1u, query_cb, this ) );
        spin();
        REQUIRE_EQ( 1u, received.size() );
        CHECK_EQ( 1u, received[0].id );
        CHECK_EQ( (uint8_t) ASRTL_FLAT_CTYPE_OBJECT, (uint8_t) received[0].value.type );
        asrt::flat_id first_child = received[0].value.data.cont.first_child;
        CHECK_NE( 0u, first_child );  // should be 2

        // Query first child of root ("sub", id=2)
        CHECK_EQ(
            ASRTL_SUCCESS,
            asrtr_param_client_fetch_any( &query, &client, first_child, query_cb, this ) );
        spin();
        REQUIRE_EQ( 2u, received.size() );
        CHECK_EQ( 2u, received[1].id );
        CHECK_EQ( "sub", received[1].key );
        CHECK_EQ( (uint8_t) ASRTL_FLAT_CTYPE_OBJECT, (uint8_t) received[1].value.type );
        asrt::flat_id sub_first_child = received[1].value.data.cont.first_child;
        asrt::flat_id sub_next_sib    = received[1].next_sibling;
        CHECK_NE( 0u, sub_first_child );  // should be 4
        CHECK_NE( 0u, sub_next_sib );     // should be 3

        // Query next sibling of "sub" → "b" (id=3)
        CHECK_EQ(
            ASRTL_SUCCESS,
            asrtr_param_client_fetch_any( &query, &client, sub_next_sib, query_cb, this ) );
        spin();
        REQUIRE_EQ( 3u, received.size() );
        CHECK_EQ( 3u, received[2].id );
        CHECK_EQ( "b", received[2].key );
        CHECK_EQ( (uint8_t) ASRTL_FLAT_STYPE_BOOL, (uint8_t) received[2].value.type );
        CHECK_EQ( 1u, received[2].value.data.s.u32_val );
        CHECK_EQ( 0u, received[2].next_sibling );  // last sibling

        // Query children of "sub": "x" (id=4)
        CHECK_EQ(
            ASRTL_SUCCESS,
            asrtr_param_client_fetch_any( &query, &client, sub_first_child, query_cb, this ) );
        spin();
        REQUIRE_EQ( 4u, received.size() );
        CHECK_EQ( 4u, received[3].id );
        CHECK_EQ( "x", received[3].key );
        CHECK_EQ( (uint8_t) ASRTL_FLAT_STYPE_U32, (uint8_t) received[3].value.type );
        CHECK_EQ( 100u, received[3].value.data.s.u32_val );
        asrt::flat_id x_next_sib = received[3].next_sibling;
        CHECK_NE( 0u, x_next_sib );  // should be 5

        // Query "y" (id=5)
        CHECK_EQ(
            ASRTL_SUCCESS,
            asrtr_param_client_fetch_any( &query, &client, x_next_sib, query_cb, this ) );
        spin();
        REQUIRE_EQ( 5u, received.size() );
        CHECK_EQ( 5u, received[4].id );
        CHECK_EQ( "y", received[4].key );
        CHECK_EQ( (uint8_t) ASRTL_FLAT_STYPE_STR, (uint8_t) received[4].value.type );
        CHECK_EQ( 0u, received[4].next_sibling );  // last sibling

        // No errors throughout
        CHECK_EQ( 0, error_called );

        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_base, "param_loopback_multi_batch" )
{
        // Small buffer (25 bytes) forces server to split siblings across batches.
        // Node wire size for "cN" U32 child: id(4)+"cN\0"(3)+type(1)+val(4) = 12
        // max_msg_size=25: msg_id(1) + 20 node space + 4 trailer → fits 1 node (12<20).
        // Actually 2×12=24>20 so only 1 fits per batch.  4 children → 4 batches.
        // Use 30 to fit 2: 30-1-4=25, 2×12=24<25 → 2 per batch, 4 children → 2 batches.
        static constexpr uint32_t SMALL_BUF = 30;

        struct asrtl_node  srv_head           = {};
        struct asrtl_node  cli_head           = {};
        stub_allocator_ctx alloc_ctx          = {};
        asrtl_allocator    alloc              = {};
        uint8_t            cli_buf[SMALL_BUF] = {};

        srv_head.chid = ASRTL_CORE;
        cli_head.chid = ASRTL_CORE;
        alloc         = asrtl_stub_allocator( &alloc_ctx );

        asrtl_sender ssend = { .ptr = this, .cb = srv_to_cli };
        asrtl_sender csend = { .ptr = this, .cb = cli_to_srv };

        REQUIRE_EQ( ASRTL_SUCCESS, asrtc_param_server_init( &server, &srv_head, ssend, alloc ) );
        struct asrtl_span mb = { .b = cli_buf, .e = cli_buf + SMALL_BUF };
        REQUIRE_EQ( ASRTL_SUCCESS, asrtr_param_client_init( &client, &cli_head, csend, mb, 100 ) );

        // Tree: root(OBJECT,1) → 4 children (U32, ids 2..5)
        struct asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append_cont( &tree, 0, 1, NULL, ASRTL_FLAT_CTYPE_OBJECT );
        asrtl_flat_tree_append_scalar( &tree, 1, 2, "c1", ASRTL_FLAT_STYPE_U32, { .u32_val = 10 } );
        asrtl_flat_tree_append_scalar( &tree, 1, 3, "c2", ASRTL_FLAT_STYPE_U32, { .u32_val = 20 } );
        asrtl_flat_tree_append_scalar( &tree, 1, 4, "c3", ASRTL_FLAT_STYPE_U32, { .u32_val = 30 } );
        asrtl_flat_tree_append_scalar( &tree, 1, 5, "c4", ASRTL_FLAT_STYPE_U32, { .u32_val = 40 } );
        asrtc_param_server_set_tree( &server, &tree );

        // Handshake
        CHECK_EQ( ASRTL_SUCCESS, asrtc_param_server_send_ready( &server, 1u, 1000, NULL, NULL ) );
        spin();
        CHECK_EQ( 1, client.ready );
        struct rn
        {
                asrt::flat_id id;
                uint32_t      val;
                asrt::flat_id next_sib;
        };
        std::vector< rn >        results;
        struct asrtr_param_query q = {};

        auto resp_cb = []( struct asrtr_param_client*,
                           struct asrtr_param_query* qq,
                           struct asrtl_flat_value   val ) {
                auto* r = (std::vector< rn >*) qq->cb_ptr;
                r->push_back( { qq->node_id, val.data.s.u32_val, qq->next_sibling } );
        };

        // Query root to get first_child
        asrt::flat_id first_child = 0;
        auto          root_resp   = []( struct asrtr_param_client*,
                             struct asrtr_param_query* qq,
                             struct asrtl_flat_value   val ) {
                *(asrt::flat_id*) qq->cb_ptr = val.data.cont.first_child;
        };
        CHECK_EQ(
            ASRTL_SUCCESS,
            asrtr_param_client_fetch_any( &q, &client, 1u, root_resp, &first_child ) );
        spin();
        REQUIRE_NE( 0u, first_child );

        // Walk all children via next_sibling_id chain
        asrt::flat_id query_id = first_child;
        while ( query_id != 0u ) {
                CHECK_EQ(
                    ASRTL_SUCCESS,
                    asrtr_param_client_fetch_any( &q, &client, query_id, resp_cb, &results ) );
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

// ============================================================================
// Phase 5 — find-by-key loopback tests
// ============================================================================

TEST_CASE_FIXTURE( param_loopback_ctx, "param_find_by_key_u32" )
{
        struct asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append_cont( &tree, 0, 1, NULL, ASRTL_FLAT_CTYPE_OBJECT );
        asrtl_flat_tree_append_scalar(
            &tree, 1, 2, "alpha", ASRTL_FLAT_STYPE_U32, { .u32_val = 42 } );
        asrtl_flat_tree_append_scalar(
            &tree, 1, 3, "beta", ASRTL_FLAT_STYPE_STR, { .str_val = "hi" } );
        asrtl_flat_tree_append_scalar(
            &tree, 1, 4, "gamma", ASRTL_FLAT_STYPE_BOOL, { .bool_val = 1 } );
        asrtc_param_server_set_tree( &server, &tree );

        CHECK_EQ( ASRTL_SUCCESS, asrtc_param_server_send_ready( &server, 1u, 1000, NULL, NULL ) );
        spin();
        REQUIRE( client.ready );

        // Find "alpha" by key in root object
        CHECK_EQ(
            ASRTL_SUCCESS,
            asrtr_param_client_find_any( &query, &client, 1u, "alpha", query_cb, this ) );
        spin();
        REQUIRE_EQ( 1u, received.size() );
        CHECK_EQ( 2u, received[0].id );
        CHECK_EQ( "alpha", std::string( received[0].key ) );
        CHECK_EQ( (uint8_t) ASRTL_FLAT_STYPE_U32, (uint8_t) received[0].value.type );
        CHECK_EQ( 42u, received[0].value.data.s.u32_val );
        CHECK_EQ( 0, error_called );

        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_loopback_ctx, "param_find_by_key_not_found" )
{
        struct asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append_cont( &tree, 0, 1, NULL, ASRTL_FLAT_CTYPE_OBJECT );
        asrtl_flat_tree_append_scalar(
            &tree, 1, 2, "alpha", ASRTL_FLAT_STYPE_U32, { .u32_val = 42 } );
        asrtc_param_server_set_tree( &server, &tree );

        CHECK_EQ( ASRTL_SUCCESS, asrtc_param_server_send_ready( &server, 1u, 1000, NULL, NULL ) );
        spin();
        REQUIRE( client.ready );

        // Find a key that doesn't exist → error callback
        CHECK_EQ(
            ASRTL_SUCCESS,
            asrtr_param_client_find_any( &query, &client, 1u, "missing", query_cb, this ) );
        spin();
        CHECK_EQ( 0u, received.size() );
        CHECK_EQ( 1, error_called );

        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_loopback_ctx, "param_find_by_key_nested" )
{
        // root (OBJECT, id=1)
        //   ├── "sub" (OBJECT, id=2)
        //   │     ├── "x" (U32=100, id=4)
        //   │     └── "y" (STR="hello", id=5)
        //   └── "b" (BOOL=1, id=3)
        struct asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append_cont( &tree, 0, 1, NULL, ASRTL_FLAT_CTYPE_OBJECT );
        asrtl_flat_tree_append_cont( &tree, 1, 2, "sub", ASRTL_FLAT_CTYPE_OBJECT );
        asrtl_flat_tree_append_scalar( &tree, 1, 3, "b", ASRTL_FLAT_STYPE_BOOL, { .bool_val = 1 } );
        asrtl_flat_tree_append_scalar( &tree, 2, 4, "x", ASRTL_FLAT_STYPE_U32, { .u32_val = 100 } );
        asrtl_flat_tree_append_scalar(
            &tree, 2, 5, "y", ASRTL_FLAT_STYPE_STR, { .str_val = "hello" } );
        asrtc_param_server_set_tree( &server, &tree );

        CHECK_EQ( ASRTL_SUCCESS, asrtc_param_server_send_ready( &server, 1u, 1000, NULL, NULL ) );
        spin();
        REQUIRE( client.ready );

        // Find "sub" in root, then find "y" inside "sub"
        CHECK_EQ(
            ASRTL_SUCCESS,
            asrtr_param_client_find_any( &query, &client, 1u, "sub", query_cb, this ) );
        spin();
        REQUIRE_EQ( 1u, received.size() );
        CHECK_EQ( 2u, received[0].id );
        CHECK_EQ( (uint8_t) ASRTL_FLAT_CTYPE_OBJECT, (uint8_t) received[0].value.type );

        asrt::flat_id sub_id = received[0].id;

        // Now find "y" inside the sub object
        CHECK_EQ(
            ASRTL_SUCCESS,
            asrtr_param_client_find_any( &query, &client, sub_id, "y", query_cb, this ) );
        spin();
        REQUIRE_EQ( 2u, received.size() );
        CHECK_EQ( 5u, received[1].id );
        CHECK_EQ( "y", std::string( received[1].key ) );
        CHECK_EQ( (uint8_t) ASRTL_FLAT_STYPE_STR, (uint8_t) received[1].value.type );

        CHECK_EQ( 0, error_called );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_loopback_ctx, "param_find_then_query" )
{
        struct asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append_cont( &tree, 0, 1, NULL, ASRTL_FLAT_CTYPE_OBJECT );
        asrtl_flat_tree_append_scalar(
            &tree, 1, 2, "alpha", ASRTL_FLAT_STYPE_U32, { .u32_val = 42 } );
        asrtl_flat_tree_append_scalar(
            &tree, 1, 3, "beta", ASRTL_FLAT_STYPE_STR, { .str_val = "hi" } );
        asrtc_param_server_set_tree( &server, &tree );

        CHECK_EQ( ASRTL_SUCCESS, asrtc_param_server_send_ready( &server, 1u, 1000, NULL, NULL ) );
        spin();
        REQUIRE( client.ready );

        // Find by key
        CHECK_EQ(
            ASRTL_SUCCESS,
            asrtr_param_client_find_any( &query, &client, 1u, "alpha", query_cb, this ) );
        spin();
        REQUIRE_EQ( 1u, received.size() );
        CHECK_EQ( 2u, received[0].id );

        // Then a normal query by id works correctly
        CHECK_EQ(
            ASRTL_SUCCESS, asrtr_param_client_fetch_any( &query, &client, 3u, query_cb, this ) );
        spin();
        REQUIRE_EQ( 2u, received.size() );
        CHECK_EQ( 3u, received[1].id );
        CHECK_EQ( "beta", std::string( received[1].key ) );

        CHECK_EQ( 0, error_called );
        asrtl_flat_tree_deinit( &tree );
}

// ============================================================================
// asrtc_collect_server — controller COLLECT channel
// ============================================================================


// Build a raw READY_ACK message; return past-end pointer.
static uint8_t* build_collect_ready_ack( uint8_t* buf )
{
        *buf = ASRTL_COLLECT_MSG_READY_ACK;
        return buf + 1;
}

// Build a raw APPEND message; return past-end pointer.
static uint8_t* build_collect_append(
    uint8_t*                       buf,
    asrt::flat_id                  parent_id,
    asrt::flat_id                  node_id,
    char const*                    key,
    struct asrtl_flat_value const* value )
{
        struct asrtl_span sp = { .b = buf, .e = buf + 256 };
        asrtl_msg_rtoc_collect_append(
            parent_id, node_id, key, value, asrtl_rec_span_to_span_cb, &sp );
        return sp.b;
}

struct collect_ctx
{
        struct asrtl_node           head      = {};
        stub_allocator_ctx          alloc_ctx = {};
        asrtl_allocator             alloc     = {};
        struct asrtc_collect_server server    = {};
        collector                   coll;
        asrtl_sender                sendr = {};
        uint32_t                    t     = 1;

        collect_ctx()
        {
                head.chid = ASRTL_CORE;
                alloc     = asrtl_stub_allocator( &alloc_ctx );
                setup_sender_collector( &sendr, &coll );
                REQUIRE_EQ(
                    ASRTL_SUCCESS,
                    asrtc_collect_server_init( &server, &head, sendr, alloc, 4, 16 ) );
        }
        ~collect_ctx() { asrtc_collect_server_deinit( &server ); }
};

TEST_CASE( "asrtc_collect_server_init" )
{
        struct asrtl_node head             = {};
        head.chid                          = ASRTL_CORE;
        asrtl_sender                null_s = {};
        struct asrtc_collect_server s      = {};

        CHECK_EQ(
            ASRTL_INIT_ERR,
            asrtc_collect_server_init( NULL, &head, null_s, asrtl_default_allocator(), 4, 16 ) );
        CHECK_EQ(
            ASRTL_INIT_ERR,
            asrtc_collect_server_init( &s, NULL, null_s, asrtl_default_allocator(), 4, 16 ) );

        CHECK_EQ(
            ASRTL_SUCCESS,
            asrtc_collect_server_init( &s, &head, null_s, asrtl_default_allocator(), 4, 16 ) );
        CHECK_EQ( ASRTL_COLL, s.node.chid );
        CHECK_NE( nullptr, (void*) (uintptr_t) s.node.e_cb_ptr );
        CHECK_EQ( &s.node, head.next );
        CHECK_EQ( ASRTC_COLLECT_SERVER_IDLE, s.state );
        asrtc_collect_server_deinit( &s );
}

TEST_CASE_FIXTURE( collect_ctx, "asrtc_collect_server_send_ready_encodes_correctly" )
{
        CHECK_EQ( ASRTL_SUCCESS, asrtc_collect_server_send_ready( &server, 1u, 1000, NULL, NULL ) );
        REQUIRE_EQ( 1u, coll.data.size() );

        auto& msg = coll.data.front();
        CHECK_EQ( ASRTL_COLL, msg.id );
        REQUIRE_EQ( 9u, msg.data.size() );
        CHECK_EQ( ASRTL_COLLECT_MSG_READY, msg.data[0] );
        assert_u32( 1u, msg.data.data() + 1 );
        assert_u32( 1u, msg.data.data() + 5 );
        CHECK_EQ( ASRTC_COLLECT_SERVER_READY_SENT, server.state );
        coll.data.pop_front();
}

TEST_CASE_FIXTURE( collect_ctx, "asrtc_collect_server_recv_ready_ack_activates" )
{
        CHECK_EQ( ASRTL_SUCCESS, asrtc_collect_server_send_ready( &server, 1u, 1000, NULL, NULL ) );
        coll.data.pop_front();

        uint8_t buf[8];
        check_recv(
            &server, (struct asrtl_span) { .b = buf, .e = build_collect_ready_ack( buf ) } );
        check_tick( &server, t++ );
        CHECK_EQ( ASRTC_COLLECT_SERVER_ACTIVE, server.state );
}

TEST_CASE_FIXTURE( collect_ctx, "asrtc_collect_server_recv_ready_ack_while_pending_returns_error" )
{
        CHECK_EQ( ASRTL_SUCCESS, asrtc_collect_server_send_ready( &server, 1u, 1000, NULL, NULL ) );
        coll.data.pop_front();

        uint8_t buf[8];
        check_recv(
            &server, (struct asrtl_span) { .b = buf, .e = build_collect_ready_ack( buf ) } );
        CHECK_EQ(
            ASRTL_RECV_ERR,
            asrtl_chann_recv(
                &server.node,
                (struct asrtl_span) { .b = buf, .e = build_collect_ready_ack( buf ) } ) );
        check_tick( &server, t++ );
}

TEST_CASE_FIXTURE( collect_ctx, "asrtc_collect_server_append_builds_tree" )
{
        CHECK_EQ( ASRTL_SUCCESS, asrtc_collect_server_send_ready( &server, 1u, 1000, NULL, NULL ) );
        coll.data.pop_front();

        // Handshake
        uint8_t buf[8];
        check_recv(
            &server, (struct asrtl_span) { .b = buf, .e = build_collect_ready_ack( buf ) } );
        check_tick( &server, t++ );

        // Append root object (parent=0, node=1, no key)
        uint8_t                 abuf[128];
        struct asrtl_flat_value obj{ .type = ASRTL_FLAT_CTYPE_OBJECT };
        uint8_t*                ae = build_collect_append( abuf, 0, 1, NULL, &obj );
        check_recv( &server, (struct asrtl_span) { .b = abuf, .e = ae } );
        check_tick( &server, t++ );

        // Append child u32 (parent=1, node=2, key="alpha", value=42)
        struct asrtl_flat_value val{ .type = ASRTL_FLAT_STYPE_U32 };
        val.data.s.u32_val = 42;
        ae                 = build_collect_append( abuf, 1, 2, "alpha", &val );
        check_recv( &server, (struct asrtl_span) { .b = abuf, .e = ae } );
        check_tick( &server, t++ );

        // Verify tree
        struct asrtl_flat_tree const*  tree = asrtc_collect_server_tree( &server );
        struct asrtl_flat_query_result qr;
        CHECK_EQ( ASRTL_SUCCESS, asrtl_flat_tree_query( (struct asrtl_flat_tree*) tree, 1, &qr ) );
        CHECK_EQ( ASRTL_FLAT_CTYPE_OBJECT, qr.value.type );

        CHECK_EQ( ASRTL_SUCCESS, asrtl_flat_tree_query( (struct asrtl_flat_tree*) tree, 2, &qr ) );
        CHECK_EQ( ASRTL_FLAT_STYPE_U32, qr.value.type );
        CHECK_EQ( 42u, qr.value.data.s.u32_val );
        CHECK_EQ( std::string( "alpha" ), std::string( qr.key ) );
}

TEST_CASE_FIXTURE( collect_ctx, "asrtc_collect_server_append_string_value" )
{
        CHECK_EQ( ASRTL_SUCCESS, asrtc_collect_server_send_ready( &server, 1u, 1000, NULL, NULL ) );
        coll.data.pop_front();

        uint8_t buf[8];
        check_recv(
            &server, (struct asrtl_span) { .b = buf, .e = build_collect_ready_ack( buf ) } );
        check_tick( &server, t++ );

        // Append root object
        uint8_t                 abuf[128];
        struct asrtl_flat_value obj{ .type = ASRTL_FLAT_CTYPE_OBJECT };
        uint8_t*                ae = build_collect_append( abuf, 0, 1, NULL, &obj );
        check_recv( &server, (struct asrtl_span) { .b = abuf, .e = ae } );
        check_tick( &server, t++ );

        // Append string child
        struct asrtl_flat_value str_val{ .type = ASRTL_FLAT_STYPE_STR };
        str_val.data.s.str_val = "hello";
        ae                     = build_collect_append( abuf, 1, 2, "msg", &str_val );
        check_recv( &server, (struct asrtl_span) { .b = abuf, .e = ae } );
        check_tick( &server, t++ );

        struct asrtl_flat_tree const*  tree = asrtc_collect_server_tree( &server );
        struct asrtl_flat_query_result qr;
        CHECK_EQ( ASRTL_SUCCESS, asrtl_flat_tree_query( (struct asrtl_flat_tree*) tree, 2, &qr ) );
        CHECK_EQ( ASRTL_FLAT_STYPE_STR, qr.value.type );
        CHECK_EQ( std::string( "hello" ), std::string( qr.value.data.s.str_val ) );
}

TEST_CASE_FIXTURE( collect_ctx, "asrtc_collect_server_append_before_active_returns_error" )
{
        uint8_t                 abuf[128];
        struct asrtl_flat_value obj{ .type = ASRTL_FLAT_CTYPE_OBJECT };
        uint8_t*                ae = build_collect_append( abuf, 0, 1, NULL, &obj );
        CHECK_EQ(
            ASRTL_RECV_ERR,
            asrtl_chann_recv( &server.node, (struct asrtl_span) { .b = abuf, .e = ae } ) );
}

TEST_CASE_FIXTURE( collect_ctx, "asrtc_collect_server_back_to_back_appends" )
{
        CHECK_EQ( ASRTL_SUCCESS, asrtc_collect_server_send_ready( &server, 1u, 1000, NULL, NULL ) );
        coll.data.pop_front();

        uint8_t buf[8];
        check_recv(
            &server, (struct asrtl_span) { .b = buf, .e = build_collect_ready_ack( buf ) } );
        check_tick( &server, t++ );

        // Append root object
        uint8_t                 abuf[128];
        struct asrtl_flat_value obj{ .type = ASRTL_FLAT_CTYPE_OBJECT };
        uint8_t*                ae = build_collect_append( abuf, 0, 1, NULL, &obj );
        check_recv( &server, (struct asrtl_span) { .b = abuf, .e = ae } );

        // Second append WITHOUT tick() in between — simulates burst arrival
        struct asrtl_flat_value val{ .type = ASRTL_FLAT_STYPE_U32 };
        val.data.s.u32_val = 42;
        ae                 = build_collect_append( abuf, 1, 2, "alpha", &val );
        check_recv( &server, (struct asrtl_span) { .b = abuf, .e = ae } );

        // Tick to finalize
        check_tick( &server, t++ );
        CHECK_EQ( ASRTC_COLLECT_SERVER_ACTIVE, server.state );

        // Verify both nodes made it into the tree
        struct asrtl_flat_tree const*  tree = asrtc_collect_server_tree( &server );
        struct asrtl_flat_query_result qr;
        CHECK_EQ( ASRTL_SUCCESS, asrtl_flat_tree_query( (struct asrtl_flat_tree*) tree, 1, &qr ) );
        CHECK_EQ( ASRTL_FLAT_CTYPE_OBJECT, qr.value.type );

        CHECK_EQ( ASRTL_SUCCESS, asrtl_flat_tree_query( (struct asrtl_flat_tree*) tree, 2, &qr ) );
        CHECK_EQ( ASRTL_FLAT_STYPE_U32, qr.value.type );
        CHECK_EQ( 42u, qr.value.data.s.u32_val );
}

TEST_CASE_FIXTURE( collect_ctx, "asrtc_collect_server_append_duplicate_sends_error" )
{
        CHECK_EQ( ASRTL_SUCCESS, asrtc_collect_server_send_ready( &server, 1u, 1000, NULL, NULL ) );
        coll.data.pop_front();

        uint8_t buf[8];
        check_recv(
            &server, (struct asrtl_span) { .b = buf, .e = build_collect_ready_ack( buf ) } );
        check_tick( &server, t++ );

        // Append root object
        uint8_t                 abuf[128];
        struct asrtl_flat_value obj{ .type = ASRTL_FLAT_CTYPE_OBJECT };
        uint8_t*                ae = build_collect_append( abuf, 0, 1, NULL, &obj );
        check_recv( &server, (struct asrtl_span) { .b = abuf, .e = ae } );
        check_tick( &server, t++ );

        // Duplicate node_id=1
        ae = build_collect_append( abuf, 0, 1, NULL, &obj );
        check_recv( &server, (struct asrtl_span) { .b = abuf, .e = ae } );
        check_tick( &server, t++ );

        // Error should have been sent and server deactivated
        CHECK_EQ( ASRTC_COLLECT_SERVER_IDLE, server.state );
        REQUIRE_GE( coll.data.size(), 1u );
        auto& err_msg = coll.data.back();
        CHECK_EQ( ASRTL_COLL, err_msg.id );
        CHECK_EQ( ASRTL_COLLECT_MSG_ERROR, err_msg.data[0] );
        coll.data.pop_back();
}

TEST_CASE_FIXTURE( collect_ctx, "asrtc_collect_server_ack_cb_fires" )
{
        enum asrtl_status ack_status = {};
        auto              ack_cb     = []( void* ptr, enum asrtl_status s ) {
                *static_cast< enum asrtl_status* >( ptr ) = s;
        };

        CHECK_EQ(
            ASRTL_SUCCESS,
            asrtc_collect_server_send_ready( &server, 1u, 1000, ack_cb, &ack_status ) );
        coll.data.pop_front();

        uint8_t buf[8];
        check_recv(
            &server, (struct asrtl_span) { .b = buf, .e = build_collect_ready_ack( buf ) } );
        check_tick( &server, t++ );
        CHECK_EQ( ASRTL_SUCCESS, ack_status );
}

TEST_CASE_FIXTURE( collect_ctx, "asrtc_collect_server_timeout_fires_ack_cb" )
{
        enum asrtl_status ack_status = {};
        auto              ack_cb     = []( void* ptr, enum asrtl_status s ) {
                *static_cast< enum asrtl_status* >( ptr ) = s;
        };

        CHECK_EQ(
            ASRTL_SUCCESS,
            asrtc_collect_server_send_ready( &server, 1u, 10, ack_cb, &ack_status ) );
        coll.data.pop_front();

        // Tick past deadline without receiving READY_ACK
        for ( uint32_t i = 0; i < 20; i++ )
                check_tick( &server, t++ );

        CHECK_EQ( ASRTL_TIMEOUT_ERR, ack_status );
}

TEST_CASE_FIXTURE( collect_ctx, "asrtc_collect_server_send_ready_double_call_returns_error" )
{
        CHECK_EQ( ASRTL_SUCCESS, asrtc_collect_server_send_ready( &server, 1u, 1000, NULL, NULL ) );
        coll.data.pop_front();

        CHECK_EQ( ASRTL_ARG_ERR, asrtc_collect_server_send_ready( &server, 1u, 1000, NULL, NULL ) );
        CHECK( coll.data.empty() );

        // Consume handshake so fixture can clean up
        uint8_t buf[8];
        check_recv(
            &server, (struct asrtl_span) { .b = buf, .e = build_collect_ready_ack( buf ) } );
        check_tick( &server, t++ );
}

TEST_CASE_FIXTURE( collect_ctx, "asrtc_collect_server_send_ready_after_timeout_allowed" )
{
        enum asrtl_status ack_status = {};
        auto              ack_cb     = []( void* ptr, enum asrtl_status s ) {
                *static_cast< enum asrtl_status* >( ptr ) = s;
        };

        CHECK_EQ(
            ASRTL_SUCCESS, asrtc_collect_server_send_ready( &server, 1u, 5, ack_cb, &ack_status ) );
        coll.data.pop_front();

        for ( uint32_t i = 0; i < 10; i++ )
                check_tick( &server, t++ );
        CHECK_EQ( ASRTL_TIMEOUT_ERR, ack_status );

        // Should be allowed to call send_ready again after timeout
        CHECK_EQ( ASRTL_SUCCESS, asrtc_collect_server_send_ready( &server, 1u, 1000, NULL, NULL ) );
        coll.data.pop_front();

        uint8_t buf[8];
        check_recv(
            &server, (struct asrtl_span) { .b = buf, .e = build_collect_ready_ack( buf ) } );
        check_tick( &server, t++ );
        CHECK_EQ( ASRTC_COLLECT_SERVER_ACTIVE, server.state );
}

TEST_CASE_FIXTURE( collect_ctx, "asrtc_collect_server_ready_ack_reinits_tree" )
{
        CHECK_EQ( ASRTL_SUCCESS, asrtc_collect_server_send_ready( &server, 1u, 1000, NULL, NULL ) );
        coll.data.pop_front();

        uint8_t buf[8];
        check_recv(
            &server, (struct asrtl_span) { .b = buf, .e = build_collect_ready_ack( buf ) } );
        check_tick( &server, t++ );

        // Append a node
        uint8_t                 abuf[128];
        struct asrtl_flat_value obj{ .type = ASRTL_FLAT_CTYPE_OBJECT };
        uint8_t*                ae = build_collect_append( abuf, 0, 1, NULL, &obj );
        check_recv( &server, (struct asrtl_span) { .b = abuf, .e = ae } );
        check_tick( &server, t++ );

        // Verify node exists
        struct asrtl_flat_tree const*  tree = asrtc_collect_server_tree( &server );
        struct asrtl_flat_query_result qr;
        CHECK_EQ( ASRTL_SUCCESS, asrtl_flat_tree_query( (struct asrtl_flat_tree*) tree, 1, &qr ) );

        // Second handshake should reinit tree (clearing old data)
        CHECK_EQ( ASRTL_SUCCESS, asrtc_collect_server_send_ready( &server, 1u, 1000, NULL, NULL ) );
        coll.data.pop_front();
        check_recv(
            &server, (struct asrtl_span) { .b = buf, .e = build_collect_ready_ack( buf ) } );
        check_tick( &server, t++ );

        // Old node should no longer exist in the fresh tree
        tree = asrtc_collect_server_tree( &server );
        CHECK_NE( ASRTL_SUCCESS, asrtl_flat_tree_query( (struct asrtl_flat_tree*) tree, 1, &qr ) );
}

TEST_CASE_FIXTURE( collect_ctx, "asrtc_collect_server_append_alloc_failure_sends_error_and_resets" )
{
        CHECK_EQ( ASRTL_SUCCESS, asrtc_collect_server_send_ready( &server, 1u, 1000, NULL, NULL ) );
        coll.data.pop_front();

        uint8_t buf[8];
        check_recv(
            &server, (struct asrtl_span) { .b = buf, .e = build_collect_ready_ack( buf ) } );
        check_tick( &server, t++ );
        CHECK_EQ( ASRTC_COLLECT_SERVER_ACTIVE, server.state );

        // Make the next allocation fail (will hit inside flat_tree_append)
        uint32_t calls_before  = alloc_ctx.alloc_calls;
        alloc_ctx.fail_at_call = calls_before + 1;

        uint8_t                 abuf[128];
        struct asrtl_flat_value obj{ .type = ASRTL_FLAT_CTYPE_OBJECT };
        uint8_t*                ae = build_collect_append( abuf, 0, 1, NULL, &obj );
        check_recv( &server, (struct asrtl_span) { .b = abuf, .e = ae } );

        // Tree alloc failure → error sent to reactor, server resets to idle
        CHECK_EQ( ASRTC_COLLECT_SERVER_IDLE, server.state );
}

using collect_base = server_client_base< asrtc_collect_server, asrtr_collect_client >;

struct collect_loopback_ctx : collect_base
{
        struct asrtl_node  srv_head  = {};
        struct asrtl_node  cli_head  = {};
        stub_allocator_ctx alloc_ctx = {};
        asrtl_allocator    alloc     = {};

        asrtl_sender srv_sendr = {};
        asrtl_sender cli_sendr = {};

        collect_loopback_ctx()
        {
                srv_head.chid = ASRTL_CORE;
                cli_head.chid = ASRTL_CORE;
                alloc         = asrtl_stub_allocator( &alloc_ctx );
                srv_sendr     = asrtl_sender{ .ptr = this, .cb = srv_to_cli };
                cli_sendr     = asrtl_sender{ .ptr = this, .cb = cli_to_srv };
                REQUIRE_EQ(
                    ASRTL_SUCCESS,
                    asrtc_collect_server_init( &server, &srv_head, srv_sendr, alloc, 4, 16 ) );
                REQUIRE_EQ(
                    ASRTL_SUCCESS, asrtr_collect_client_init( &client, &cli_head, cli_sendr ) );
        }

        ~collect_loopback_ctx() { asrtc_collect_server_deinit( &server ); }

        void handshake( asrt::flat_id root_id = 1u )
        {
                REQUIRE_EQ(
                    ASRTL_SUCCESS,
                    asrtc_collect_server_send_ready( &server, root_id, 1000, NULL, NULL ) );
                spin();
                REQUIRE_EQ( ASRTC_COLLECT_SERVER_ACTIVE, server.state );
                REQUIRE_EQ( ASRTR_COLLECT_CLIENT_ACTIVE, client.state );
        }
};

TEST_CASE_FIXTURE( collect_loopback_ctx, "collect_loopback_handshake" )
{
        handshake();
        CHECK_EQ( 1u, asrtr_collect_client_root_id( &client ) );
}

TEST_CASE_FIXTURE( collect_loopback_ctx, "collect_loopback_multi_level_tree" )
{
        // Build tree via reactor:
        //   root (OBJECT, id=1)
        //     ├── "nums" (ARRAY, id=2)
        //     │     ├── (U32=10, id=3)
        //     │     └── (U32=20, id=4)
        //     ├── "name" (STR="hello", id=5)
        //     └── "flag" (BOOL=1, id=6)
        handshake( 1u );

        // parent_id=0 = virtual root for top-level nodes.
        // Server must tick between appends to process each pending APPEND.
        asrt::flat_id root, nums;
        CHECK_EQ( ASRTL_SUCCESS, asrtr_collect_client_append_object( &client, 0, NULL, &root ) );
        check_tick( &server, t++ );

        CHECK_EQ(
            ASRTL_SUCCESS, asrtr_collect_client_append_array( &client, root, "nums", &nums ) );
        check_tick( &server, t++ );

        CHECK_EQ( ASRTL_SUCCESS, asrtr_collect_client_append_u32( &client, nums, NULL, 10 ) );
        check_tick( &server, t++ );

        CHECK_EQ( ASRTL_SUCCESS, asrtr_collect_client_append_u32( &client, nums, NULL, 20 ) );
        check_tick( &server, t++ );

        CHECK_EQ(
            ASRTL_SUCCESS, asrtr_collect_client_append_str( &client, root, "name", "hello" ) );
        check_tick( &server, t++ );

        CHECK_EQ( ASRTL_SUCCESS, asrtr_collect_client_append_bool( &client, root, "flag", 1 ) );
        check_tick( &server, t++ );

        // Verify the tree built on the controller side
        struct asrtl_flat_tree const*  tree = asrtc_collect_server_tree( &server );
        struct asrtl_flat_query_result qr;

        // Root object (id=1)
        CHECK_EQ( ASRTL_SUCCESS, asrtl_flat_tree_query( (struct asrtl_flat_tree*) tree, 1, &qr ) );
        CHECK_EQ( ASRTL_FLAT_CTYPE_OBJECT, qr.value.type );

        // "nums" array (id=2)
        CHECK_EQ( ASRTL_SUCCESS, asrtl_flat_tree_query( (struct asrtl_flat_tree*) tree, 2, &qr ) );
        CHECK_EQ( ASRTL_FLAT_CTYPE_ARRAY, qr.value.type );
        CHECK_EQ( std::string( "nums" ), std::string( qr.key ) );

        // Array children: U32=10 (id=3), U32=20 (id=4)
        CHECK_EQ( ASRTL_SUCCESS, asrtl_flat_tree_query( (struct asrtl_flat_tree*) tree, 3, &qr ) );
        CHECK_EQ( ASRTL_FLAT_STYPE_U32, qr.value.type );
        CHECK_EQ( 10u, qr.value.data.s.u32_val );

        CHECK_EQ( ASRTL_SUCCESS, asrtl_flat_tree_query( (struct asrtl_flat_tree*) tree, 4, &qr ) );
        CHECK_EQ( ASRTL_FLAT_STYPE_U32, qr.value.type );
        CHECK_EQ( 20u, qr.value.data.s.u32_val );

        // "name" = "hello" (id=5)
        CHECK_EQ( ASRTL_SUCCESS, asrtl_flat_tree_query( (struct asrtl_flat_tree*) tree, 5, &qr ) );
        CHECK_EQ( ASRTL_FLAT_STYPE_STR, qr.value.type );
        CHECK_EQ( std::string( "hello" ), std::string( qr.value.data.s.str_val ) );
        CHECK_EQ( std::string( "name" ), std::string( qr.key ) );

        // "flag" = true (id=6)
        CHECK_EQ( ASRTL_SUCCESS, asrtl_flat_tree_query( (struct asrtl_flat_tree*) tree, 6, &qr ) );
        CHECK_EQ( ASRTL_FLAT_STYPE_BOOL, qr.value.type );
        CHECK_EQ( 1u, qr.value.data.s.bool_val );
        CHECK_EQ( std::string( "flag" ), std::string( qr.key ) );
}

TEST_CASE_FIXTURE( collect_loopback_ctx, "collect_loopback_duplicate_node_sends_error" )
{
        handshake();

        // Append first node (parent=0 = virtual root)
        CHECK_EQ( ASRTL_SUCCESS, asrtr_collect_client_append_object( &client, 0, NULL, NULL ) );
        check_tick( &server, t++ );

        // The client auto-assigns node_ids starting at 1.  The second append
        // gets id=2 — which is unique, so we cannot trigger a duplicate via the
        // public API alone.  Instead, send a raw APPEND with the same node_id=1
        // through the loopback to test the server ERROR path.
        struct asrtl_flat_value dup{ .type = ASRTL_FLAT_STYPE_U32 };
        dup.data.s.u32_val = 42;
        uint8_t           buf[256];
        uint8_t*          end = build_collect_append( buf, 0, 1, "dup", &dup );
        struct asrtl_span msg = { .b = buf, .e = end };
        check_recv( &server, msg );
        check_tick( &server, t++ );

        // Server should have sent ERROR and gone idle
        CHECK_EQ( ASRTC_COLLECT_SERVER_IDLE, server.state );

        // Client should have received the ERROR and be in error state
        // (the cli_to_srv / srv_to_cli wiring delivers it synchronously)
        CHECK_EQ( ASRTR_COLLECT_CLIENT_ERROR, client.state );
}

TEST_CASE_FIXTURE( collect_loopback_ctx, "collect_loopback_append_after_error_rejected" )
{
        handshake();

        // Append one valid node (parent=0 = virtual root)
        CHECK_EQ( ASRTL_SUCCESS, asrtr_collect_client_append_object( &client, 0, NULL, NULL ) );
        check_tick( &server, t++ );

        // Force duplicate via raw message
        struct asrtl_flat_value dup{ .type = ASRTL_FLAT_STYPE_U32 };
        dup.data.s.u32_val = 1;
        uint8_t           buf[256];
        uint8_t*          end = build_collect_append( buf, 0, 1, "x", &dup );
        struct asrtl_span msg = { .b = buf, .e = end };
        check_recv( &server, msg );
        check_tick( &server, t++ );

        CHECK_EQ( ASRTR_COLLECT_CLIENT_ERROR, client.state );

        // Further appends from the client should fail
        CHECK_EQ( ASRTL_ARG_ERR, asrtr_collect_client_append_u32( &client, 0, "y", 99 ) );
}

// =====================================================================
// stream server tests
// =====================================================================

#include "../asrtc/stream.h"
#include "../asrtr/stream.h"
#include "./stub_allocator.hpp"

/// Fixture for isolated controller-side stream server tests.
struct strm_server_ctx
{
        collector           coll;
        asrtl_sender        sender = {};
        stub_allocator_ctx  alloc_ctx;
        asrtl_allocator     alloc  = {};
        asrtl_node          root   = {};
        asrtc_stream_server server = {};

        strm_server_ctx()
        {
                setup_sender_collector( &sender, &coll );
                alloc = asrtl_stub_allocator( &alloc_ctx );
                CHECK_EQ(
                    ASRTL_SUCCESS, asrtc_stream_server_init( &server, &root, sender, alloc ) );
        }
        ~strm_server_ctx()
        {
                asrtc_stream_server_deinit( &server );
                coll.data.clear();
        }
};

/// Helper: build a raw DEFINE message and deliver it to the server's recv_cb.
static enum asrtl_status strm_deliver_define(
    asrtc_stream_server*                server,
    uint8_t                             schema_id,
    enum asrtl_strm_field_type_e const* fields,
    uint8_t                             field_count )
{
        // [MSG_DEFINE, schema_id, field_count, fields...]
        std::vector< uint8_t > buf;
        buf.push_back( ASRTL_STRM_MSG_DEFINE );
        buf.push_back( schema_id );
        buf.push_back( field_count );
        for ( uint8_t i = 0; i < field_count; i++ )
                buf.push_back( static_cast< uint8_t >( fields[i] ) );
        struct asrtl_span sp = { .b = buf.data(), .e = buf.data() + buf.size() };
        return asrtl_chann_recv( &server->node, sp );
}

/// Helper: build a raw DATA message and deliver it.
static enum asrtl_status strm_deliver_data(
    asrtc_stream_server* server,
    uint8_t              schema_id,
    uint8_t const*       data,
    uint16_t             size )
{
        std::vector< uint8_t > buf;
        buf.push_back( ASRTL_STRM_MSG_DATA );
        buf.push_back( schema_id );
        buf.insert( buf.end(), data, data + size );
        struct asrtl_span sp = { .b = buf.data(), .e = buf.data() + buf.size() };
        return asrtl_chann_recv( &server->node, sp );
}

// --- init ---

TEST_CASE( "strm_server_init: null server" )
{
        asrtl_node      root   = {};
        asrtl_sender    sender = {};
        asrtl_allocator alloc  = {};
        CHECK_EQ( ASRTL_INIT_ERR, asrtc_stream_server_init( nullptr, &root, sender, alloc ) );
}

TEST_CASE( "strm_server_init: null prev" )
{
        asrtc_stream_server server = {};
        asrtl_sender        sender = {};
        asrtl_allocator     alloc  = {};
        CHECK_EQ( ASRTL_INIT_ERR, asrtc_stream_server_init( &server, nullptr, sender, alloc ) );
}

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_init: valid" )
{
        CHECK_EQ( ASRTL_STRM, server.node.chid );
        CHECK_EQ( &server.node, root.next );
}

// --- recv: empty buffer ---

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_recv: empty buffer" )
{
        uint8_t           buf[1];
        struct asrtl_span sp = { .b = buf, .e = buf };
        check_recv( &server, sp );
}

// --- recv: unknown message ---

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_recv: unknown message id" )
{
        uint8_t           buf[] = { 0xFF };
        struct asrtl_span sp    = { .b = buf, .e = buf + 1 };
        CHECK_EQ( ASRTL_RECV_UNEXPECTED_ERR, asrtl_chann_recv( &server.node, sp ) );
}

// --- DEFINE handling ---

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_define: valid single field" )
{
        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_U32 };
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_define( &server, 0, fields, 1 ) );
        CHECK_NE( nullptr, server.lookup[0] );
        CHECK_EQ( 0, server.lookup[0]->schema_id );
        CHECK_EQ( 1, server.lookup[0]->field_count );
        CHECK_EQ( 4, server.lookup[0]->record_size );
        CHECK_EQ( 0u, server.lookup[0]->count );
}

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_define: multi-field record_size" )
{
        enum asrtl_strm_field_type_e fields[] = {
            ASRTL_STRM_FIELD_U8, ASRTL_STRM_FIELD_U16, ASRTL_STRM_FIELD_FLOAT };
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_define( &server, 10, fields, 3 ) );
        // 1+2+4 = 7
        CHECK_EQ( 7, server.lookup[10]->record_size );
}

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_define: structural fields have zero size" )
{
        enum asrtl_strm_field_type_e fields[] = {
            ASRTL_STRM_FIELD_LBRACKET, ASRTL_STRM_FIELD_U8, ASRTL_STRM_FIELD_RBRACKET };
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_define( &server, 1, fields, 3 ) );
        // 0+1+0 = 1
        CHECK_EQ( 1, server.lookup[1]->record_size );
}

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_define: truncated header" )
{
        // Only message id, no schema_id or field_count
        uint8_t           buf[] = { ASRTL_STRM_MSG_DEFINE };
        struct asrtl_span sp    = { .b = buf, .e = buf + 1 };
        CHECK_EQ( ASRTL_RECV_ERR, asrtl_chann_recv( &server.node, sp ) );
}

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_define: zero field_count sends error" )
{
        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_U8 };
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_define( &server, 0, fields, 0 ) );
        // Server should have sent an ERROR message back
        REQUIRE_EQ( 1u, coll.data.size() );
        CHECK_EQ( ASRTL_STRM, coll.data[0].id );
        CHECK_EQ( ASRTL_STRM_MSG_ERROR, coll.data[0].data[0] );
        CHECK_EQ( ASRTL_STRM_ERR_INVALID_DEFINE, coll.data[0].data[1] );
        coll.data.clear();
}

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_define: duplicate schema sends error" )
{
        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_U8 };
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_define( &server, 5, fields, 1 ) );
        // Second define with same schema_id
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_define( &server, 5, fields, 1 ) );
        REQUIRE_EQ( 1u, coll.data.size() );
        CHECK_EQ( ASRTL_STRM_MSG_ERROR, coll.data[0].data[0] );
        CHECK_EQ( ASRTL_STRM_ERR_DUPLICATE_SCHEMA, coll.data[0].data[1] );
        coll.data.clear();
}

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_define: invalid field type tag" )
{
        // Build raw message with an invalid tag
        uint8_t           buf[] = { ASRTL_STRM_MSG_DEFINE, 0, 1, 0xFF };
        struct asrtl_span sp    = { .b = buf, .e = buf + 4 };
        check_recv( &server, sp );
        REQUIRE_EQ( 1u, coll.data.size() );
        CHECK_EQ( ASRTL_STRM_MSG_ERROR, coll.data[0].data[0] );
        CHECK_EQ( ASRTL_STRM_ERR_INVALID_DEFINE, coll.data[0].data[1] );
        coll.data.clear();
}

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_define: truncated field tags" )
{
        // Says 3 fields but only provides 1 byte of tags
        uint8_t           buf[] = { ASRTL_STRM_MSG_DEFINE, 0, 3, ASRTL_STRM_FIELD_U8 };
        struct asrtl_span sp    = { .b = buf, .e = buf + 4 };
        check_recv( &server, sp );
        REQUIRE_EQ( 1u, coll.data.size() );
        CHECK_EQ( ASRTL_STRM_MSG_ERROR, coll.data[0].data[0] );
        CHECK_EQ( ASRTL_STRM_ERR_INVALID_DEFINE, coll.data[0].data[1] );
        coll.data.clear();
}

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_define: alloc failure on schema" )
{
        alloc_ctx.fail_at_call                = 1;  // fail first alloc (schema struct)
        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_U8 };
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_define( &server, 0, fields, 1 ) );
        REQUIRE_EQ( 1u, coll.data.size() );
        CHECK_EQ( ASRTL_STRM_MSG_ERROR, coll.data[0].data[0] );
        CHECK_EQ( ASRTL_STRM_ERR_ALLOC_FAILURE, coll.data[0].data[1] );
        coll.data.clear();
}

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_define: alloc failure on fields array" )
{
        alloc_ctx.fail_at_call                = 2;  // pass schema alloc, fail fields alloc
        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_U8 };
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_define( &server, 0, fields, 1 ) );
        REQUIRE_EQ( 1u, coll.data.size() );
        CHECK_EQ( ASRTL_STRM_MSG_ERROR, coll.data[0].data[0] );
        CHECK_EQ( ASRTL_STRM_ERR_ALLOC_FAILURE, coll.data[0].data[1] );
        coll.data.clear();
}

// --- DATA handling ---

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_data: unknown schema" )
{
        uint8_t data[] = { 0x42 };
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_data( &server, 10, data, 1 ) );
        REQUIRE_EQ( 1u, coll.data.size() );
        CHECK_EQ( ASRTL_STRM_MSG_ERROR, coll.data[0].data[0] );
        CHECK_EQ( ASRTL_STRM_ERR_UNKNOWN_SCHEMA, coll.data[0].data[1] );
        coll.data.clear();
}

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_data: size mismatch" )
{
        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_U32 };  // record_size=4
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_define( &server, 0, fields, 1 ) );
        uint8_t data[] = { 1, 2 };  // only 2 bytes, not 4
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_data( &server, 0, data, 2 ) );
        REQUIRE_EQ( 1u, coll.data.size() );
        CHECK_EQ( ASRTL_STRM_MSG_ERROR, coll.data[0].data[0] );
        CHECK_EQ( ASRTL_STRM_ERR_SIZE_MISMATCH, coll.data[0].data[1] );
        coll.data.clear();
}

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_data: valid single record" )
{
        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_U8 };
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_define( &server, 0, fields, 1 ) );

        uint8_t data[] = { 0xAB };
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_data( &server, 0, data, 1 ) );

        auto* schema = server.lookup[0];
        REQUIRE_NE( nullptr, schema );
        CHECK_EQ( 1u, schema->count );
        REQUIRE_NE( nullptr, schema->first );
        CHECK_EQ( 0xAB, schema->first->data[0] );
        CHECK_EQ( schema->first, schema->last );
}

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_data: multiple records linked list" )
{
        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_U8 };
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_define( &server, 0, fields, 1 ) );

        for ( uint8_t i = 0; i < 5; i++ )
                CHECK_EQ( ASRTL_SUCCESS, strm_deliver_data( &server, 0, &i, 1 ) );

        auto* schema = server.lookup[0];
        CHECK_EQ( 5u, schema->count );

        // Walk the linked list
        auto* rec = schema->first;
        for ( uint8_t i = 0; i < 5; i++ ) {
                REQUIRE_NE( nullptr, rec );
                CHECK_EQ( i, rec->data[0] );
                rec = rec->next;
        }
        CHECK_EQ( nullptr, rec );
}

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_data: alloc failure on record node" )
{
        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_U8 };
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_define( &server, 0, fields, 1 ) );

        // schema alloc=1, fields alloc=2 already done; next is record node=3
        alloc_ctx.fail_at_call = 3;
        uint8_t data[]         = { 0x01 };
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_data( &server, 0, data, 1 ) );
        REQUIRE_EQ( 1u, coll.data.size() );
        CHECK_EQ( ASRTL_STRM_MSG_ERROR, coll.data[0].data[0] );
        CHECK_EQ( ASRTL_STRM_ERR_ALLOC_FAILURE, coll.data[0].data[1] );
        coll.data.clear();
}

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_data: alloc failure on data buffer" )
{
        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_U8 };
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_define( &server, 0, fields, 1 ) );

        // schema=1, fields=2, record node=3 OK, data buf=4 FAIL
        alloc_ctx.fail_at_call = 4;
        uint8_t data[]         = { 0x01 };
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_data( &server, 0, data, 1 ) );
        REQUIRE_EQ( 1u, coll.data.size() );
        CHECK_EQ( ASRTL_STRM_MSG_ERROR, coll.data[0].data[0] );
        CHECK_EQ( ASRTL_STRM_ERR_ALLOC_FAILURE, coll.data[0].data[1] );
        coll.data.clear();
}

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_data: truncated (missing schema_id)" )
{
        uint8_t           buf[] = { ASRTL_STRM_MSG_DATA };
        struct asrtl_span sp    = { .b = buf, .e = buf + 1 };
        CHECK_EQ( ASRTL_RECV_ERR, asrtl_chann_recv( &server.node, sp ) );
}

// --- take ---

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_take: empty returns zero" )
{
        auto result = asrtc_stream_server_take( &server );
        CHECK_EQ( 0u, result.schema_count );
        CHECK_EQ( nullptr, result.schemas );
}

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_take: takes ownership, server cleared" )
{
        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_U8 };
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_define( &server, 3, fields, 1 ) );
        uint8_t data[] = { 42 };
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_data( &server, 3, data, 1 ) );

        auto result = asrtc_stream_server_take( &server );
        REQUIRE_EQ( 1u, result.schema_count );
        CHECK_EQ( 3, result.schemas[0].schema_id );
        CHECK_EQ( 1u, result.schemas[0].count );
        CHECK_EQ( 42, result.schemas[0].first->data[0] );

        // Server lookup should be cleared
        CHECK_EQ( nullptr, server.lookup[3] );

        asrtc_stream_schemas_free( &result );
}

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_take: multiple schemas" )
{
        enum asrtl_strm_field_type_e f1[] = { ASRTL_STRM_FIELD_U8 };
        enum asrtl_strm_field_type_e f2[] = { ASRTL_STRM_FIELD_U16 };
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_define( &server, 0, f1, 1 ) );
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_define( &server, 1, f2, 1 ) );

        auto result = asrtc_stream_server_take( &server );
        REQUIRE_EQ( 2u, result.schema_count );
        // Schemas are ordered by ID (scan from 0)
        CHECK_EQ( 0, result.schemas[0].schema_id );
        CHECK_EQ( 1, result.schemas[1].schema_id );
        CHECK_EQ( 1, result.schemas[0].record_size );
        CHECK_EQ( 2, result.schemas[1].record_size );

        asrtc_stream_schemas_free( &result );
}

// --- clear ---

TEST_CASE_FIXTURE( strm_server_ctx, "strm_server_clear: frees all schemas and records" )
{
        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_U8 };
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_define( &server, 0, fields, 1 ) );
        uint8_t data[] = { 1 };
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_data( &server, 0, data, 1 ) );

        asrtc_stream_server_clear( &server );
        CHECK_EQ( nullptr, server.lookup[0] );

        // After clear, can re-define same schema
        CHECK_EQ( ASRTL_SUCCESS, strm_deliver_define( &server, 0, fields, 1 ) );
        CHECK_NE( nullptr, server.lookup[0] );
}

// --- loopback: reactor client → controller server ---

struct strm_loopback_ctx : server_client_base< asrtc_stream_server, asrtr_stream_client >
{
        collector  coll;
        asrtl_node root_r = {};
        asrtl_node root_c = {};

        stub_allocator_ctx alloc_ctx;

        asrtl_sender sender_r = { .ptr = nullptr, .cb = cli_to_srv };
        asrtl_sender sender_c = { .ptr = nullptr, .cb = srv_to_cli };

        strm_loopback_ctx()
        {
                sender_r.ptr = this;
                sender_c.ptr = this;
                auto alloc   = asrtl_stub_allocator( &alloc_ctx );
                CHECK_EQ( ASRTL_SUCCESS, asrtr_stream_client_init( &client, &root_r, sender_r ) );
                CHECK_EQ(
                    ASRTL_SUCCESS, asrtc_stream_server_init( &server, &root_c, sender_c, alloc ) );
        }
        ~strm_loopback_ctx()
        {
                asrtc_stream_server_deinit( &server );
                coll.data.clear();
        }
};

TEST_CASE_FIXTURE( strm_loopback_ctx, "strm_loopback: define and verify schema received" )
{
        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_U8, ASRTL_STRM_FIELD_FLOAT };
        asrtl_status                 done_st  = {};
        auto                         done_cb  = []( void* p, enum asrtl_status s ) {
                *static_cast< asrtl_status* >( p ) = s;
        };
        CHECK_EQ(
            ASRTL_SUCCESS, asrtr_stream_client_define( &client, 1, fields, 2, done_cb, &done_st ) );
        check_tick( &client, 1 );
        CHECK_EQ( ASRTR_STRM_DONE, client.state );
        check_tick( &client, 2 );
        CHECK_EQ( ASRTR_STRM_IDLE, client.state );
        CHECK_EQ( ASRTL_SUCCESS, done_st );

        // Verify server received the schema
        auto* schema = server.lookup[1];
        REQUIRE_NE( nullptr, schema );
        CHECK_EQ( 1, schema->schema_id );
        CHECK_EQ( 2, schema->field_count );
        CHECK_EQ( 5, schema->record_size );  // 1 + 4
}

TEST_CASE_FIXTURE( strm_loopback_ctx, "strm_loopback: define + record + take" )
{
        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_U8 };
        CHECK_EQ(
            ASRTL_SUCCESS, asrtr_stream_client_define( &client, 0, fields, 1, nullptr, nullptr ) );
        check_tick( &client, 1 );
        check_tick( &client, 2 );

        // Send 3 records
        for ( uint8_t i = 10; i < 13; i++ ) {
                CHECK_EQ(
                    ASRTL_SUCCESS,
                    asrtr_stream_client_emit( &client, 0, &i, 1, nullptr, nullptr ) );
                check_tick( &client, 3 + i - 10 );
        }

        // Take from server
        auto result = asrtc_stream_server_take( &server );
        REQUIRE_EQ( 1u, result.schema_count );
        CHECK_EQ( 3u, result.schemas[0].count );

        auto* rec = result.schemas[0].first;
        for ( uint8_t i = 10; i < 13; i++ ) {
                REQUIRE_NE( nullptr, rec );
                CHECK_EQ( i, rec->data[0] );
                rec = rec->next;
        }

        asrtc_stream_schemas_free( &result );
}

TEST_CASE_FIXTURE( strm_loopback_ctx, "strm_loopback: duplicate define → error propagation" )
{
        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_U8 };
        // First define succeeds
        CHECK_EQ(
            ASRTL_SUCCESS, asrtr_stream_client_define( &client, 0, fields, 1, nullptr, nullptr ) );
        check_tick( &client, 1 );
        check_tick( &client, 2 );
        CHECK_EQ( ASRTR_STRM_IDLE, client.state );

        // Second define with same ID → controller sends ERROR → client enters ERROR
        CHECK_EQ(
            ASRTL_SUCCESS, asrtr_stream_client_define( &client, 0, fields, 1, nullptr, nullptr ) );
        check_tick( &client, 1 );
        CHECK_EQ( ASRTR_STRM_ERROR, client.state );
        CHECK_EQ( ASRTL_STRM_ERR_DUPLICATE_SCHEMA, client.err_code );
}

TEST_CASE_FIXTURE( strm_loopback_ctx, "strm_loopback: data size mismatch → error" )
{
        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_U32 };  // expects 4 bytes
        CHECK_EQ(
            ASRTL_SUCCESS, asrtr_stream_client_define( &client, 0, fields, 1, nullptr, nullptr ) );
        check_tick( &client, 1 );
        check_tick( &client, 2 );

        // Send 1 byte instead of 4 → server sends error → client enters ERROR
        uint8_t bad_data[] = { 0x01 };
        CHECK_EQ(
            ASRTL_SUCCESS, asrtr_stream_client_emit( &client, 0, bad_data, 1, nullptr, nullptr ) );
        CHECK_EQ( ASRTR_STRM_ERROR, client.state );
        CHECK_EQ( ASRTL_STRM_ERR_SIZE_MISMATCH, client.err_code );
}

TEST_CASE_FIXTURE( strm_loopback_ctx, "strm_loopback: record after error rejected" )
{
        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_U32 };
        CHECK_EQ(
            ASRTL_SUCCESS, asrtr_stream_client_define( &client, 0, fields, 1, nullptr, nullptr ) );
        check_tick( &client, 1 );
        check_tick( &client, 2 );

        // Force error
        uint8_t bad[] = { 0x01 };
        asrtr_stream_client_emit( &client, 0, bad, 1, nullptr, nullptr );
        CHECK_EQ( ASRTR_STRM_ERROR, client.state );

        // Further records should fail
        uint8_t good[] = { 1, 2, 3, 4 };
        CHECK_EQ(
            ASRTL_INTERNAL_ERR, asrtr_stream_client_emit( &client, 0, good, 4, nullptr, nullptr ) );
}

TEST_CASE_FIXTURE( strm_loopback_ctx, "strm_loopback: reset after error, redefine" )
{
        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_U32 };
        CHECK_EQ(
            ASRTL_SUCCESS, asrtr_stream_client_define( &client, 0, fields, 1, nullptr, nullptr ) );
        check_tick( &client, 1 );
        check_tick( &client, 2 );

        // Force error
        uint8_t bad[] = { 0x01 };
        asrtr_stream_client_emit( &client, 0, bad, 1, nullptr, nullptr );
        CHECK_EQ( ASRTR_STRM_ERROR, client.state );

        // Reset client
        CHECK_EQ( ASRTL_SUCCESS, asrtr_stream_client_reset( &client ) );
        CHECK_EQ( ASRTR_STRM_IDLE, client.state );

        // Clear server and redefine
        asrtc_stream_server_clear( &server );
        enum asrtl_strm_field_type_e fields2[] = { ASRTL_STRM_FIELD_U8 };
        CHECK_EQ(
            ASRTL_SUCCESS, asrtr_stream_client_define( &client, 0, fields2, 1, nullptr, nullptr ) );
        check_tick( &client, 1 );
        check_tick( &client, 2 );
        CHECK_EQ( ASRTR_STRM_IDLE, client.state );

        uint8_t good[] = { 42 };
        CHECK_EQ(
            ASRTL_SUCCESS, asrtr_stream_client_emit( &client, 0, good, 1, nullptr, nullptr ) );
        check_tick( &client, 1 );

        auto result = asrtc_stream_server_take( &server );
        REQUIRE_EQ( 1u, result.schema_count );
        CHECK_EQ( 1u, result.schemas[0].count );
        CHECK_EQ( 42, result.schemas[0].first->data[0] );
        asrtc_stream_schemas_free( &result );
}
