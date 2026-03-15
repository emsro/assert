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
#include "../asrtl/core_proto.h"
#include "../asrtl/ecode.h"
#include "../asrtl/log.h"
#include "../asrtl/proto_version.h"
#include "../asrtr/diag.h"
#include "../asrtr/reactor.h"
#include "./asrtr_tests.h"
#include "./collector.hpp"
#include "./util.h"

#include <algorithm>
#include <doctest/doctest.h>

ASRTL_DEFINE_GPOS_LOG()

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
        CHECK_EQ( ASRTR_SUCCESS, st );
        st = asrtr_reactor_add_test( r, t );
        CHECK_EQ( ASRTR_SUCCESS, st );
}

void check_reactor_init( struct asrtr_reactor* reac, struct asrtl_sender sender, char const* desc )
{
        enum asrtr_status st = asrtr_reactor_init( reac, sender, desc );
        CHECK_EQ( ASRTR_SUCCESS, st );
}

void check_diag_init( struct asrtr_diag* diag, struct asrtl_node* prev, struct asrtl_sender sender )
{
        enum asrtr_status st = asrtr_diag_init( diag, prev, sender );
        CHECK_EQ( ASRTR_SUCCESS, st );
}

void check_reactor_recv( struct asrtr_reactor* reac, struct asrtl_span msg )
{
        enum asrtl_status st = asrtr_reactor_recv( reac, msg );
        CHECK_EQ( ASRTL_SUCCESS, st );
}

void check_reactor_recv_flags( struct asrtr_reactor* reac, struct asrtl_span msg, uint32_t flags )
{
        check_reactor_recv( reac, msg );
        CHECK_EQ( flags, reac->flags & ~ASRTR_PASSIVE_FLAGS );
}

void check_reactor_tick( struct asrtr_reactor* reac )
{
        enum asrtr_status st = asrtr_reactor_tick( reac );
        CHECK_EQ( ASRTR_SUCCESS, st );
        CHECK_EQ( 0x00, reac->flags & ~ASRTR_PASSIVE_FLAGS );
}

void check_recv_and_spin(
    struct asrtr_reactor*    reac,
    uint8_t*                 beg,
    uint8_t*                 end,
    enum asrtr_reactor_flags fls )
{
        check_reactor_recv_flags( reac, (struct asrtl_span) { beg, end }, fls );
        int       i = 0;
        int const n = 1000;
        for ( ; i < n; i++ ) {
                check_reactor_tick( reac );
                if ( reac->state == ASRTR_REAC_IDLE )
                        break;
        }
        CHECK_NE( i, n );
}

void check_run_test( struct asrtr_reactor* reac, uint32_t test_id, uint32_t run_id )
{
        uint8_t           buffer[64];
        struct asrtl_span sp = { .b = buffer, .e = buffer + sizeof buffer };
        enum asrtl_status st =
            asrtl_msg_ctor_test_start( test_id, run_id, asrtl_rec_span_to_span_cb, &sp );
        CHECK_EQ( ASRTL_SUCCESS, st );
        check_recv_and_spin( reac, buffer, sp.b, ASRTR_FLAG_TSTART );
}

void assert_diag_record_any_line( struct collected_data& collected )
{
        assert_collected_diag_hdr( collected, ASRTL_DIAG_MSG_RECORD );
        uint32_t line = 0;
        asrtl_u8d4_to_u32( collected.data.data() + 1, &line );
        CHECK( line >= 1 );
        CHECK( collected.data.size() > 5 );
        auto* fn_begin = collected.data.data() + 5;
        auto* fn_end   = collected.data.data() + collected.data.size();
        CHECK( std::none_of( fn_begin, fn_end, []( uint8_t b ) {
                return b == '\0';
        } ) );
}

void assert_diag_record( struct collected_data& collected, uint32_t line )
{
        assert_collected_diag_hdr( collected, ASRTL_DIAG_MSG_RECORD );
        assert_u32( line, collected.data.data() + 1 );
        CHECK( collected.data.size() > 5 );
        auto* fn_begin = collected.data.data() + 5;
        auto* fn_end   = collected.data.data() + collected.data.size();
        CHECK( std::none_of( fn_begin, fn_end, []( uint8_t b ) {
                return b == '\0';
        } ) );
}

void assert_test_result(
    struct collected_data&   collected,
    uint32_t                 id,
    enum asrtl_test_result_e result )
{
        assert_collected_core_hdr( collected, 0x08, ASRTL_MSG_TEST_RESULT );
        assert_u32( id, collected.data.data() + 2 );
        assert_u16( result, collected.data.data() + 6 );
}

void assert_test_start( struct collected_data& collected, uint16_t test_id, uint32_t run_id )
{
        assert_collected_core_hdr( collected, 0x08, ASRTL_MSG_TEST_START );
        assert_u16( test_id, collected.data.data() + 2 );
        assert_u32( run_id, collected.data.data() + 4 );
}

struct reactor_ctx
{
        struct asrtr_reactor reac = {};
        collector            coll;
        struct asrtl_sender  send       = {};
        uint8_t              buffer[64] = {};
        struct asrtl_span    sp         = {};

        reactor_ctx()
        {
                sp = { buffer, buffer + sizeof buffer };
                setup_sender_collector( &send, &coll );
        }
        ~reactor_ctx()
        {
                CHECK_EQ( coll.data.empty(), true );
        }
};

//---------------------------------------------------------------------
// tests

enum asrtr_status dataless_test_fun( struct asrtr_record* x )
{
        (void) x;
        return ASRTR_SUCCESS;
}

TEST_CASE_FIXTURE( reactor_ctx, "reactor_init" )
{
        enum asrtr_status st;

        st = asrtr_reactor_init( NULL, send, "rec1" );
        CHECK_EQ( ASRTR_INIT_ERR, st );

        st = asrtr_reactor_init( &reac, send, NULL );
        CHECK_EQ( ASRTR_INIT_ERR, st );

        st = asrtr_reactor_init( &reac, send, "rec1" );
        CHECK_EQ( reac.first_test, nullptr );
        CHECK_EQ( reac.node.chid, ASRTL_CORE );
        CHECK_EQ( reac.state, ASRTR_REAC_IDLE );

        struct asrtr_test t1, t2;
        st = asrtr_test_init( &t1, NULL, NULL, &dataless_test_fun );
        CHECK_EQ( st, ASRTR_TEST_INIT_ERR );
        st = asrtr_test_init( &t1, "test1", NULL, NULL );
        CHECK_EQ( st, ASRTR_TEST_INIT_ERR );

        setup_test( &reac, &t1, "test1", NULL, &dataless_test_fun );
        setup_test( &reac, &t2, "test2", NULL, &dataless_test_fun );

        CHECK_EQ( t2.next, nullptr );
        CHECK_EQ( t1.next, &t2 );
        CHECK_EQ( reac.first_test, &t1 );
}

TEST_CASE_FIXTURE( reactor_ctx, "reactor_version" )
{
        check_reactor_init( &reac, send, "rec1" );

        asrtl_msg_ctor_proto_version( asrtl_rec_span_to_span_cb, &sp );

        check_recv_and_spin( &reac, buffer, sp.b, ASRTR_FLAG_PROTO_VER );

        auto& collected = coll.data.back();
        assert_collected_core_hdr( collected, 0x08, ASRTL_MSG_PROTO_VERSION );
        assert_u16( ASRTL_PROTO_MAJOR, collected.data.data() + 2 );
        assert_u16( ASRTL_PROTO_MINOR, collected.data.data() + 4 );
        assert_u16( ASRTL_PROTO_PATCH, collected.data.data() + 6 );

        coll.data.pop_back();
}

TEST_CASE_FIXTURE( reactor_ctx, "reactor_desc" )
{
        check_reactor_init( &reac, send, "rec1" );

        asrtl_msg_ctor_desc( asrtl_rec_span_to_span_cb, &sp );

        check_recv_and_spin( &reac, buffer, sp.b, ASRTR_FLAG_DESC );

        auto& collected = coll.data.back();
        assert_collected_core_hdr( collected, 0x06, ASRTL_MSG_DESC );
        assert_data_ll_contain_str( "rec1", collected, 2 );

        coll.data.pop_back();
}

TEST_CASE_FIXTURE( reactor_ctx, "reactor_test_count" )
{
        check_reactor_init( &reac, send, "rec1" );

        asrtl_msg_ctor_test_count( asrtl_rec_span_to_span_cb, &sp );

        check_recv_and_spin( &reac, buffer, sp.b, ASRTR_FLAG_TC );

        {
                auto& collected = coll.data.back();
                assert_collected_core_hdr( collected, 0x04, ASRTL_MSG_TEST_COUNT );
                assert_u16( 0x00, collected.data.data() + 2 );
                coll.data.pop_back();
        }

        // re-init to add a test before any recv
        check_reactor_init( &reac, send, "rec1" );
        struct asrtr_test t1;
        setup_test( &reac, &t1, "test1", NULL, &dataless_test_fun );

        check_recv_and_spin( &reac, buffer, sp.b, ASRTR_FLAG_TC );

        {
                auto& collected = coll.data.back();
                assert_collected_core_hdr( collected, 0x04, ASRTL_MSG_TEST_COUNT );
                assert_u16( 0x01, collected.data.data() + 2 );
                coll.data.pop_back();
        }
}

TEST_CASE_FIXTURE( reactor_ctx, "reactor_test_info" )
{
        check_reactor_init( &reac, send, "rec1" );

        asrtl_msg_ctor_test_info( 0, asrtl_rec_span_to_span_cb, &sp );

        check_recv_and_spin( &reac, buffer, sp.b, ASRTR_FLAG_TI );

        {
                auto& collected = coll.data.back();
                assert_collected_core_hdr( collected, 0x04, ASRTL_MSG_ERROR );
                assert_u16( ASRTL_ASE_MISSING_TEST, collected.data.data() + 2 );
                coll.data.pop_back();
        }

        // re-init to add a test before any recv
        check_reactor_init( &reac, send, "rec1" );
        struct asrtr_test t1;
        setup_test( &reac, &t1, "test1", NULL, &dataless_test_fun );

        check_recv_and_spin( &reac, buffer, sp.b, ASRTR_FLAG_TI );

        {
                auto& collected = coll.data.back();
                assert_collected_core_hdr( collected, 0x09, ASRTL_MSG_TEST_INFO );
                assert_u16( 0x00, collected.data.data() + 2 );
                assert_data_ll_contain_str( "test1", collected, 4 );
                coll.data.pop_back();
        }
}

TEST_CASE_FIXTURE( reactor_ctx, "reactor_start" )
{
        check_reactor_init( &reac, send, "rec1" );

        struct asrtr_test      t1;
        struct insta_test_data data = { .state = ASRTR_TEST_PASS, .counter = 0 };
        setup_test( &reac, &t1, "test1", &data, &insta_test_fun );

        // just run one test
        check_run_test( &reac, 0, 0 );
        CHECK_EQ( 1, data.counter );

        {
                auto& collected = coll.data.back();
                assert_test_result( collected, 0, ASRTL_TEST_SUCCESS );
                coll.data.pop_back();
        }

        {
                auto& collected = coll.data.back();
                assert_test_start( collected, 0, 0 );
                coll.data.pop_back();
        }

        asrtl_msg_ctor_test_start( 42, 0, asrtl_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &reac, buffer, sp.b, ASRTR_FLAG_TSTART );

        CHECK_EQ( 1, data.counter );
        {
                auto& collected = coll.data.back();
                assert_collected_core_hdr( collected, 4, ASRTL_MSG_ERROR );
                assert_u16( ASRTL_ASE_MISSING_TEST, collected.data.data() + 2 );
                coll.data.pop_back();
        }
}

TEST_CASE_FIXTURE( reactor_ctx, "reactor_start_busy" )
{
        check_reactor_init( &reac, send, "rec1" );

        struct asrtr_test t1;
        uint64_t          counter = 8;
        setup_test( &reac, &t1, "test1", &counter, &countdown_test );

        asrtl_msg_ctor_test_start( 0, 0, asrtl_rec_span_to_span_cb, &sp );
        check_reactor_recv_flags( &reac, (struct asrtl_span) { buffer, sp.b }, ASRTR_FLAG_TSTART );

        check_reactor_tick( &reac );
        CHECK_EQ( 8, counter );
        check_reactor_tick( &reac );
        CHECK_EQ( 7, counter );

        {
                auto& collected = coll.data.back();
                assert_test_start( collected, 0, 0 );
                coll.data.pop_back();
        }

        check_reactor_recv_flags( &reac, (struct asrtl_span) { buffer, sp.b }, ASRTR_FLAG_TSTART );

        check_reactor_tick( &reac );
        CHECK_EQ( 7, counter );

        {
                auto& collected = coll.data.back();
                assert_collected_core_hdr( collected, 0x04, ASRTL_MSG_ERROR );
                assert_u16( ASRTL_ASE_TEST_ALREADY_RUNNING, collected.data.data() + 2 );
                coll.data.pop_back();
        }
}

TEST_CASE_FIXTURE( reactor_ctx, "check_macro" )
{
        check_reactor_init( &reac, send, "rec1" );
        struct asrtr_test t1;
        struct asrtr_diag diag;
        check_diag_init( &diag, &reac.node, send );
        struct astrt_check_ctx check_ctx = {
            .diag    = &diag,
            .counter = 0,
        };
        setup_test( &reac, &t1, "test1", &check_ctx, &check_macro_test );

        check_run_test( &reac, 0, 0 );

        CHECK_EQ( 2, check_ctx.counter );

        {
                auto& collected = coll.data.back();
                assert_test_result( collected, 0, ASRTL_TEST_FAILURE );
                coll.data.pop_back();
        }

        {
                auto& collected = coll.data.back();
                assert_diag_record( collected, 38 );
                coll.data.pop_back();
        }

        {
                auto& collected = coll.data.back();
                assert_test_start( collected, 0, 0 );
                coll.data.pop_back();
        }
}

TEST_CASE_FIXTURE( reactor_ctx, "require_macro" )
{
        check_reactor_init( &reac, send, "rec1" );
        struct asrtr_test t1;
        struct asrtr_diag diag;
        check_diag_init( &diag, &reac.node, send );
        struct astrt_check_ctx check_ctx = {
            .diag    = &diag,
            .counter = 0,
        };
        setup_test( &reac, &t1, "test1", &check_ctx, &require_macro_test );

        check_run_test( &reac, 0, 0 );

        CHECK_EQ( 1, check_ctx.counter );
        {
                auto& collected = coll.data.back();
                assert_test_result( collected, 0, ASRTL_TEST_FAILURE );
                coll.data.pop_back();
        }

        {
                auto& collected = coll.data.back();
                assert_diag_record( collected, 28 );
                coll.data.pop_back();
        }

        {
                auto& collected = coll.data.back();
                assert_test_start( collected, 0, 0 );
                coll.data.pop_back();
        }
}

TEST_CASE_FIXTURE( reactor_ctx, "test_counter" )
{
        check_reactor_init( &reac, send, "rec1" );

        struct asrtr_test t1;
        uint64_t          counter = 0;
        setup_test( &reac, &t1, "test1", &counter, &countdown_test );

        for ( uint32_t x = 0; x < 42; x++ ) {
                counter = 1;
                check_run_test( &reac, 0, x );
                {
                        auto& collected = coll.data.back();
                        assert_test_result( collected, x, ASRTL_TEST_SUCCESS );
                        coll.data.pop_back();
                }

                {
                        auto& collected = coll.data.back();
                        assert_test_start( collected, 0, x );
                        coll.data.pop_back();
                }
        }
}

TEST_CASE_FIXTURE( reactor_ctx, "reactor_unknown_flag" )
{
        CHECK_EQ( ASRTR_SUCCESS, asrtr_reactor_init( &reac, send, "desc" ) );
        // set only unknown flag bits (known flags are 0x01..0x20); the else branch must signal an
        // error
        reac.flags           = 0x40;
        enum asrtr_status st = asrtr_reactor_tick( &reac );
        CHECK_NE( ASRTR_SUCCESS, st );
}

// R03: duplicate TEST_INFO before tick must be rejected
TEST_CASE_FIXTURE( reactor_ctx, "reactor_test_info_repeat" )
{
        check_reactor_init( &reac, send, "rec1" );

        struct asrtr_test t1;
        setup_test( &reac, &t1, "test1", NULL, &dataless_test_fun );

        asrtl_msg_ctor_test_info( 0, asrtl_rec_span_to_span_cb, &sp );

        // first recv — flag must be set
        check_reactor_recv_flags( &reac, (struct asrtl_span) { buffer, sp.b }, ASRTR_FLAG_TI );

        // second recv before tick — must be rejected
        enum asrtl_status st = asrtr_reactor_recv( &reac, (struct asrtl_span) { buffer, sp.b } );
        CHECK_EQ( ASRTL_RECV_UNEXPECTED_ERR, st );

        // flag must still be set
        CHECK_EQ( ASRTR_FLAG_TI, reac.flags & ~ASRTR_PASSIVE_FLAGS );
}

// R03: duplicate TEST_START before tick must be rejected
TEST_CASE_FIXTURE( reactor_ctx, "reactor_test_start_repeat" )
{
        check_reactor_init( &reac, send, "rec1" );

        struct asrtr_test t1;
        setup_test( &reac, &t1, "test1", NULL, &dataless_test_fun );

        asrtl_msg_ctor_test_start( 0, 42, asrtl_rec_span_to_span_cb, &sp );

        // first recv — flag must be set
        check_reactor_recv_flags( &reac, (struct asrtl_span) { buffer, sp.b }, ASRTR_FLAG_TSTART );

        // second recv before tick — must be rejected
        enum asrtl_status st = asrtr_reactor_recv( &reac, (struct asrtl_span) { buffer, sp.b } );
        CHECK_EQ( ASRTL_RECV_UNEXPECTED_ERR, st );

        // flag must still be set
        CHECK_EQ( ASRTR_FLAG_TSTART, reac.flags & ~ASRTR_PASSIVE_FLAGS );
}

// R04: add_test must be rejected after the first recv call
TEST_CASE_FIXTURE( reactor_ctx, "reactor_add_test_after_recv" )
{
        check_reactor_init( &reac, send, "rec1" );

        struct asrtr_test t1, t2;
        setup_test( &reac, &t1, "test1", NULL, &dataless_test_fun );

        // any valid recv locks registration
        asrtl_msg_ctor_proto_version( asrtl_rec_span_to_span_cb, &sp );
        check_reactor_recv( &reac, (struct asrtl_span) { buffer, sp.b } );

        // adding a test after recv must be rejected
        enum asrtr_status st = asrtr_test_init( &t2, "test2", NULL, &dataless_test_fun );
        CHECK_EQ( ASRTR_SUCCESS, st );
        st = asrtr_reactor_add_test( &reac, &t2 );
        CHECK_EQ( ASRTR_TEST_REG_ERR, st );

        // test list must not have grown
        CHECK_EQ( t1.next, nullptr );
}

// R-cov1: continue_f returning an error sets ASRTR_TEST_ERROR → sends ASRTL_TEST_ERROR result
TEST_CASE_FIXTURE( reactor_ctx, "reactor_test_error" )
{
        check_reactor_init( &reac, send, "rec1" );

        struct asrtr_test t1;
        setup_test( &reac, &t1, "test1", NULL, &error_continue_fun );

        check_run_test( &reac, 0, 0 );

        {
                auto& collected = coll.data.back();
                assert_test_result( collected, 0, ASRTL_TEST_ERROR );
                coll.data.pop_back();
        }

        {
                auto& collected = coll.data.back();
                assert_test_start( collected, 0, 0 );
                coll.data.pop_back();
        }
}

// R-cov2: two requests received before any tick — both flags set simultaneously
TEST_CASE_FIXTURE( reactor_ctx, "reactor_multi_flag" )
{
        check_reactor_init( &reac, send, "rec1" );

        // First request: test count
        asrtl_msg_ctor_test_count( asrtl_rec_span_to_span_cb, &sp );
        uint8_t* end1 = sp.b;
        check_reactor_recv( &reac, (struct asrtl_span) { buffer, end1 } );

        // Second request: description (no tick between)
        sp.b = buffer;
        asrtl_msg_ctor_desc( asrtl_rec_span_to_span_cb, &sp );
        uint8_t* end2 = sp.b;
        check_reactor_recv( &reac, (struct asrtl_span) { buffer, end2 } );

        // Both flags must be set at the same time
        CHECK( ( reac.flags & ASRTR_FLAG_TC ) );
        CHECK( ( reac.flags & ASRTR_FLAG_DESC ) );

        // First tick: DESC handled (highest priority in if-else chain)
        enum asrtr_status st = asrtr_reactor_tick( &reac );
        CHECK_EQ( ASRTR_SUCCESS, st );
        {
                auto& collected = coll.data.back();
                assert_collected_core_hdr( collected, 0x06, ASRTL_MSG_DESC );
                assert_data_ll_contain_str( "rec1", collected, 2 );
                coll.data.pop_back();
        }
        CHECK( ( reac.flags & ASRTR_FLAG_TC ) );
        CHECK( !( reac.flags & ASRTR_FLAG_DESC ) );

        // Second tick: TC handled
        st = asrtr_reactor_tick( &reac );
        CHECK_EQ( ASRTR_SUCCESS, st );
        {
                auto& collected = coll.data.back();
                assert_collected_core_hdr( collected, 0x04, ASRTL_MSG_TEST_COUNT );
                assert_u16( 0x00, collected.data.data() + 2 );
                coll.data.pop_back();
        }
        CHECK( !( reac.flags & ASRTR_FLAG_TC ) );
}

// R-INIT-1..4
TEST_CASE_FIXTURE( reactor_ctx, "diag_init" )
{
        struct asrtr_diag diag = {};

        // R-INIT-1: NULL diag
        CHECK_EQ( ASRTR_INIT_ERR, asrtr_diag_init( NULL, &reac.node, send ) );

        // R-INIT-2: NULL prev
        CHECK_EQ( ASRTR_INIT_ERR, asrtr_diag_init( &diag, NULL, send ) );

        // R-INIT-3 / R-INIT-4: valid init
        check_reactor_init( &reac, send, "rec1" );
        CHECK_EQ( ASRTR_SUCCESS, asrtr_diag_init( &diag, &reac.node, send ) );
        CHECK_EQ( ASRTL_DIAG, diag.node.chid );
        CHECK_EQ( &diag.node, reac.node.next );  // node appended after prev
        CHECK_EQ( nullptr, diag.node.next );      // chain-terminal
}

// R-REC-1..3  (R-REC-4 is verified inside assert_diag_record via size > 5 + no-null)
TEST_CASE_FIXTURE( reactor_ctx, "diag_record" )
{
        check_reactor_init( &reac, send, "rec1" );
        struct asrtr_diag diag = {};
        check_diag_init( &diag, &reac.node, send );

        // R-REC-1: normal call — verify full byte content
        asrtr_diag_record( &diag, "test.c", 42 );
        {
                auto& collected = coll.data.back();
                assert_diag_record( collected, 42 );
                CHECK_EQ( 1u + 4u + 6u, collected.data.size() );  // 1+4+strlen("test.c")
                assert_data_ll_contain_str( "test.c", collected, 5 );
                coll.data.pop_back();
        }

        // R-REC-2: line = 0
        asrtr_diag_record( &diag, "f.c", 0 );
        {
                auto& collected = coll.data.back();
                assert_diag_record( collected, 0 );
                coll.data.pop_back();
        }

        // R-REC-3: line = UINT32_MAX
        asrtr_diag_record( &diag, "f.c", UINT32_MAX );
        {
                auto& collected = coll.data.back();
                assert_diag_record( collected, UINT32_MAX );
                coll.data.pop_back();
        }
}

// R-CHK-3: two consecutive CHECK failures → two diag messages, counter incremented twice
TEST_CASE_FIXTURE( reactor_ctx, "check_macro_two_fails" )
{
        check_reactor_init( &reac, send, "rec1" );
        struct asrtr_test      t1;
        struct asrtr_diag      diag;
        check_diag_init( &diag, &reac.node, send );
        struct astrt_check_ctx check_ctx = { .diag = &diag, .counter = 0 };
        setup_test( &reac, &t1, "test1", &check_ctx, &check_macro_two_fails );

        check_run_test( &reac, 0, 0 );

        CHECK_EQ( 2, check_ctx.counter );
        {
                auto& collected = coll.data.back();
                assert_test_result( collected, 0, ASRTL_TEST_FAILURE );
                coll.data.pop_back();
        }
        {
                auto& collected = coll.data.back();
                assert_diag_record_any_line( collected );
                coll.data.pop_back();
        }
        {
                auto& collected = coll.data.back();
                assert_diag_record_any_line( collected );
                coll.data.pop_back();
        }
        {
                auto& collected = coll.data.back();
                assert_test_start( collected, 0, 0 );
                coll.data.pop_back();
        }
}

// R-CHK-4: one CHECK failure then one pass → one diag message, counter = 2
TEST_CASE_FIXTURE( reactor_ctx, "check_macro_fail_pass" )
{
        check_reactor_init( &reac, send, "rec1" );
        struct asrtr_test      t1;
        struct asrtr_diag      diag;
        check_diag_init( &diag, &reac.node, send );
        struct astrt_check_ctx check_ctx = { .diag = &diag, .counter = 0 };
        setup_test( &reac, &t1, "test1", &check_ctx, &check_macro_fail_pass );

        check_run_test( &reac, 0, 0 );

        CHECK_EQ( 2, check_ctx.counter );
        {
                auto& collected = coll.data.back();
                assert_test_result( collected, 0, ASRTL_TEST_FAILURE );
                coll.data.pop_back();
        }
        {
                auto& collected = coll.data.back();
                assert_diag_record_any_line( collected );
                coll.data.pop_back();
        }
        {
                auto& collected = coll.data.back();
                assert_test_start( collected, 0, 0 );
                coll.data.pop_back();
        }
}

// R-REQ-4: failing REQUIRE → code after it unreachable
TEST_CASE_FIXTURE( reactor_ctx, "require_fail_then_check" )
{
        check_reactor_init( &reac, send, "rec1" );
        struct asrtr_test      t1;
        struct asrtr_diag      diag;
        check_diag_init( &diag, &reac.node, send );
        struct astrt_check_ctx check_ctx = { .diag = &diag, .counter = 0 };
        setup_test( &reac, &t1, "test1", &check_ctx, &require_then_check );

        check_run_test( &reac, 0, 0 );

        CHECK_EQ( 0, check_ctx.counter );  // counter never incremented
        {
                auto& collected = coll.data.back();
                assert_test_result( collected, 0, ASRTL_TEST_FAILURE );
                coll.data.pop_back();
        }
        {
                auto& collected = coll.data.back();
                assert_diag_record_any_line( collected );  // only one record (REQUIRE)
                coll.data.pop_back();
        }
        {
                auto& collected = coll.data.back();
                assert_test_start( collected, 0, 0 );
                coll.data.pop_back();
        }
}

// R-MIX-1: CHECK fails, REQUIRE passes, CHECK fails → two diag messages, counter = 3
TEST_CASE_FIXTURE( reactor_ctx, "mix_check_require_check" )
{
        check_reactor_init( &reac, send, "rec1" );
        struct asrtr_test      t1;
        struct asrtr_diag      diag;
        check_diag_init( &diag, &reac.node, send );
        struct astrt_check_ctx check_ctx = { .diag = &diag, .counter = 0 };
        setup_test( &reac, &t1, "test1", &check_ctx, &mix_check_require_check );

        check_run_test( &reac, 0, 0 );

        CHECK_EQ( 3, check_ctx.counter );
        {
                auto& collected = coll.data.back();
                assert_test_result( collected, 0, ASRTL_TEST_FAILURE );
                coll.data.pop_back();
        }
        {
                auto& collected = coll.data.back();
                assert_diag_record_any_line( collected );  // second CHECK failure
                coll.data.pop_back();
        }
        {
                auto& collected = coll.data.back();
                assert_diag_record_any_line( collected );  // first CHECK failure
                coll.data.pop_back();
        }
        {
                auto& collected = coll.data.back();
                assert_test_start( collected, 0, 0 );
                coll.data.pop_back();
        }
}

// R-MIX-2: CHECK fails, REQUIRE fails → two diag messages, counter = 1
TEST_CASE_FIXTURE( reactor_ctx, "mix_check_require_fail" )
{
        check_reactor_init( &reac, send, "rec1" );
        struct asrtr_test      t1;
        struct asrtr_diag      diag;
        check_diag_init( &diag, &reac.node, send );
        struct astrt_check_ctx check_ctx = { .diag = &diag, .counter = 0 };
        setup_test( &reac, &t1, "test1", &check_ctx, &mix_check_require_fail );

        check_run_test( &reac, 0, 0 );

        CHECK_EQ( 1, check_ctx.counter );  // second increment unreachable
        {
                auto& collected = coll.data.back();
                assert_test_result( collected, 0, ASRTL_TEST_FAILURE );
                coll.data.pop_back();
        }
        {
                auto& collected = coll.data.back();
                assert_diag_record_any_line( collected );  // REQUIRE diag
                coll.data.pop_back();
        }
        {
                auto& collected = coll.data.back();
                assert_diag_record_any_line( collected );  // CHECK diag
                coll.data.pop_back();
        }
        {
                auto& collected = coll.data.back();
                assert_test_start( collected, 0, 0 );
                coll.data.pop_back();
        }
}

// R-cov3: truncated and trailing-byte recv errors in asrtr_reactor_recv
TEST_CASE_FIXTURE( reactor_ctx, "reactor_recv_truncated" )
{
        check_reactor_init( &reac, send, "rec1" );

        uint8_t           buf[16];
        struct asrtl_span sp;
        enum asrtl_status rst;

        // Truncated TEST_INFO: only message ID, no u16 tid
        sp = (struct asrtl_span) { .b = buf, .e = buf + sizeof buf };
        asrtl_add_u16( &sp.b, ASRTL_MSG_TEST_INFO );
        rst = asrtr_reactor_recv( &reac, (struct asrtl_span) { .b = buf, .e = sp.b } );
        CHECK_EQ( ASRTL_RECV_ERR, rst );

        // Truncated TEST_START: only ID + partial tid(2), missing run_id(4)
        sp = (struct asrtl_span) { .b = buf, .e = buf + sizeof buf };
        asrtl_add_u16( &sp.b, ASRTL_MSG_TEST_START );
        asrtl_add_u16( &sp.b, 0 );  // tid only, no run_id
        rst = asrtr_reactor_recv( &reac, (struct asrtl_span) { .b = buf, .e = sp.b } );
        CHECK_EQ( ASRTL_RECV_ERR, rst );

        // Trailing bytes: PROTO_VERSION request + extra bytes
        sp = (struct asrtl_span) { .b = buf, .e = buf + sizeof buf };
        asrtl_add_u16( &sp.b, ASRTL_MSG_PROTO_VERSION );
        asrtl_add_u16( &sp.b, 0xFFFF );  // extra bytes after a no-payload message
        rst = asrtr_reactor_recv( &reac, (struct asrtl_span) { .b = buf, .e = sp.b } );
        CHECK_EQ( ASRTL_RECV_ERR, rst );
}
