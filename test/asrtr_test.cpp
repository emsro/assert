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
#include "../asrtl/log.h"
#include "../asrtl/proto_version.h"
#include "../asrtlpp/flat_type_traits.hpp"
#include "../asrtr/collect.h"
#include "../asrtr/diag.h"
#include "../asrtr/param.h"
#include "../asrtr/reactor.h"
#include "./asrtr_tests.h"
#include "./collector.hpp"
#include "./util.h"

#include <algorithm>
#include <doctest/doctest.h>

ASRT_DEFINE_GPOS_LOG()

//---------------------------------------------------------------------
// lib

void setup_test(
    struct asrt_reactor* r,
    struct asrt_test*    t,
    char const*          name,
    void*                data,
    asrt_test_callback   start_f )
{
        assert( r );
        assert( t );
        enum asrt_status st = asrt_test_init( t, name, data, start_f );
        CHECK_EQ( ASRT_SUCCESS, st );
        st = asrt_reactor_add_test( r, t );
        CHECK_EQ( ASRT_SUCCESS, st );
}

void check_reactor_init( struct asrt_reactor* reac, struct asrt_sender sender, char const* desc )
{
        enum asrt_status st = asrt_reactor_init( reac, sender, desc );
        CHECK_EQ( ASRT_SUCCESS, st );
}

void check_diag_init(
    struct asrt_diag_client* diag,
    struct asrt_node*        prev,
    struct asrt_sender       sender )
{
        enum asrt_status st = asrt_diag_client_init( diag, prev, sender );
        CHECK_EQ( ASRT_SUCCESS, st );
}

void check_reactor_recv( struct asrt_reactor* reac, struct asrt_span msg )
{
        enum asrt_status st = asrt_chann_recv( &reac->node, msg );
        CHECK_EQ( ASRT_SUCCESS, st );
}

void check_reactor_recv_flags( struct asrt_reactor* reac, struct asrt_span msg, uint32_t flags )
{
        check_reactor_recv( reac, msg );
        CHECK_EQ( flags, reac->flags & ~ASRT_PASSIVE_FLAGS );
}

void check_reactor_tick( struct asrt_reactor* reac )
{
        enum asrt_status st = asrt_chann_tick( &reac->node, 0 );
        CHECK_EQ( ASRT_SUCCESS, st );
        CHECK_EQ( 0x00, reac->flags & ~ASRT_PASSIVE_FLAGS );
}

void check_recv_and_spin(
    struct asrt_reactor*    reac,
    uint8_t*                beg,
    uint8_t*                end,
    enum asrt_reactor_flags fls )
{
        check_reactor_recv_flags( reac, ( struct asrt_span ){ beg, end }, fls );
        int       i = 0;
        int const n = 1000;
        for ( ; i < n; i++ ) {
                check_reactor_tick( reac );
                if ( reac->state == ASRT_REAC_IDLE )
                        break;
        }
        CHECK_NE( i, n );
}

void check_run_test( struct asrt_reactor* reac, uint32_t test_id, uint32_t run_id )
{
        uint8_t          buffer[64];
        struct asrt_span sp = { .b = buffer, .e = buffer + sizeof buffer };
        enum asrt_status st =
            asrt_msg_ctor_test_start( test_id, run_id, asrt_rec_span_to_span_cb, &sp );
        CHECK_EQ( ASRT_SUCCESS, st );
        check_recv_and_spin( reac, buffer, sp.b, ASRT_FLAG_TSTART );
}

void assert_diag_record_any_line( struct collected_data& collected )
{
        assert_collected_diag_hdr( collected, ASRT_DIAG_MSG_RECORD );
        uint32_t line = 0;
        asrt_u8d4_to_u32( collected.data.data() + 1, &line );
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
        assert_collected_diag_hdr( collected, ASRT_DIAG_MSG_RECORD );
        assert_u32( line, collected.data.data() + 1 );
        CHECK( collected.data.size() > 5 );
        auto* fn_begin = collected.data.data() + 5;
        auto* fn_end   = collected.data.data() + collected.data.size();
        CHECK( std::none_of( fn_begin, fn_end, []( uint8_t b ) {
                return b == '\0';
        } ) );
}

void assert_test_result(
    struct collected_data&  collected,
    uint32_t                id,
    enum asrt_test_result_e result )
{
        assert_collected_core_hdr( collected, 0x08, ASRT_MSG_TEST_RESULT );
        assert_u32( id, collected.data.data() + 2 );
        assert_u16( result, collected.data.data() + 6 );
}

void assert_test_start( struct collected_data& collected, uint16_t test_id, uint32_t run_id )
{
        assert_collected_core_hdr( collected, 0x08, ASRT_MSG_TEST_START );
        assert_u16( test_id, collected.data.data() + 2 );
        assert_u32( run_id, collected.data.data() + 4 );
}

struct reactor_ctx
{
        struct asrt_reactor reac = {};
        collector           coll;
        struct asrt_sender  send       = {};
        uint8_t             buffer[64] = {};
        struct asrt_span    sp         = {};

        reactor_ctx()
        {
                sp = { buffer, buffer + sizeof buffer };
                setup_sender_collector( &send, &coll );
        }
        ~reactor_ctx() { CHECK_EQ( coll.data.empty(), true ); }
};

//---------------------------------------------------------------------
// tests

enum asrt_status dataless_test_fun( struct asrt_record* x )
{
        (void) x;
        return ASRT_SUCCESS;
}

TEST_CASE_FIXTURE( reactor_ctx, "reactor_init" )
{
        enum asrt_status st;

        st = asrt_reactor_init( NULL, send, "rec1" );
        CHECK_EQ( ASRT_INIT_ERR, st );

        st = asrt_reactor_init( &reac, send, NULL );
        CHECK_EQ( ASRT_INIT_ERR, st );

        st = asrt_reactor_init( &reac, send, "rec1" );
        CHECK_EQ( reac.first_test, nullptr );
        CHECK_EQ( reac.node.chid, ASRT_CORE );
        CHECK_EQ( reac.state, ASRT_REAC_IDLE );

        struct asrt_test t1, t2;
        st = asrt_test_init( &t1, NULL, NULL, &dataless_test_fun );
        CHECK_EQ( st, ASRT_INIT_ERR );
        st = asrt_test_init( &t1, "test1", NULL, NULL );
        CHECK_EQ( st, ASRT_INIT_ERR );

        setup_test( &reac, &t1, "test1", NULL, &dataless_test_fun );
        setup_test( &reac, &t2, "test2", NULL, &dataless_test_fun );

        CHECK_EQ( t2.next, nullptr );
        CHECK_EQ( t1.next, &t2 );
        CHECK_EQ( reac.first_test, &t1 );
}

TEST_CASE_FIXTURE( reactor_ctx, "reactor_version" )
{
        check_reactor_init( &reac, send, "rec1" );

        asrt_msg_ctor_proto_version( asrt_rec_span_to_span_cb, &sp );

        check_recv_and_spin( &reac, buffer, sp.b, ASRT_FLAG_PROTO_VER );

        auto& collected = coll.data.back();
        assert_collected_core_hdr( collected, 0x08, ASRT_MSG_PROTO_VERSION );
        assert_u16( ASRT_PROTO_MAJOR, collected.data.data() + 2 );
        assert_u16( ASRT_PROTO_MINOR, collected.data.data() + 4 );
        assert_u16( ASRT_PROTO_PATCH, collected.data.data() + 6 );

        coll.data.pop_back();
}

TEST_CASE_FIXTURE( reactor_ctx, "reactor_desc" )
{
        check_reactor_init( &reac, send, "rec1" );

        asrt_msg_ctor_desc( asrt_rec_span_to_span_cb, &sp );

        check_recv_and_spin( &reac, buffer, sp.b, ASRT_FLAG_DESC );

        auto& collected = coll.data.back();
        assert_collected_core_hdr( collected, 0x06, ASRT_MSG_DESC );
        assert_data_ll_contain_str( "rec1", collected, 2 );

        coll.data.pop_back();
}

TEST_CASE_FIXTURE( reactor_ctx, "reactor_test_count" )
{
        check_reactor_init( &reac, send, "rec1" );

        asrt_msg_ctor_test_count( asrt_rec_span_to_span_cb, &sp );

        check_recv_and_spin( &reac, buffer, sp.b, ASRT_FLAG_TC );

        {
                auto& collected = coll.data.back();
                assert_collected_core_hdr( collected, 0x04, ASRT_MSG_TEST_COUNT );
                assert_u16( 0x00, collected.data.data() + 2 );
                coll.data.pop_back();
        }

        // re-init to add a test before any recv
        check_reactor_init( &reac, send, "rec1" );
        struct asrt_test t1;
        setup_test( &reac, &t1, "test1", NULL, &dataless_test_fun );

        check_recv_and_spin( &reac, buffer, sp.b, ASRT_FLAG_TC );

        {
                auto& collected = coll.data.back();
                assert_collected_core_hdr( collected, 0x04, ASRT_MSG_TEST_COUNT );
                assert_u16( 0x01, collected.data.data() + 2 );
                coll.data.pop_back();
        }
}

TEST_CASE_FIXTURE( reactor_ctx, "reactor_test_info" )
{
        check_reactor_init( &reac, send, "rec1" );

        asrt_msg_ctor_test_info( 0, asrt_rec_span_to_span_cb, &sp );

        check_recv_and_spin( &reac, buffer, sp.b, ASRT_FLAG_TI );

        {
                auto& collected = coll.data.back();
                assert_collected_core_hdr( collected, 0x05, ASRT_MSG_TEST_INFO );
                assert_u16( 0x00, collected.data.data() + 2 );
                CHECK_EQ( ASRT_TEST_INFO_MISSING_TEST_ERR, collected.data[4] );
                CHECK_EQ( 5u, collected.data.size() );
                coll.data.pop_back();
        }

        // re-init to add a test before any recv
        check_reactor_init( &reac, send, "rec1" );
        struct asrt_test t1;
        setup_test( &reac, &t1, "test1", NULL, &dataless_test_fun );

        check_recv_and_spin( &reac, buffer, sp.b, ASRT_FLAG_TI );

        {
                auto& collected = coll.data.back();
                assert_collected_core_hdr( collected, 0x0A, ASRT_MSG_TEST_INFO );
                assert_u16( 0x00, collected.data.data() + 2 );
                CHECK_EQ( ASRT_TEST_INFO_SUCCESS, collected.data[4] );
                assert_data_ll_contain_str( "test1", collected, 5 );
                coll.data.pop_back();
        }
}

TEST_CASE_FIXTURE( reactor_ctx, "reactor_start" )
{
        check_reactor_init( &reac, send, "rec1" );

        struct asrt_test       t1;
        struct insta_test_data data = { .state = ASRT_TEST_PASS, .counter = 0 };
        setup_test( &reac, &t1, "test1", &data, &insta_test_fun );

        // just run one test
        check_run_test( &reac, 0, 0 );
        CHECK_EQ( 1, data.counter );

        {
                auto& collected = coll.data.back();
                assert_test_result( collected, 0, ASRT_TEST_RESULT_SUCCESS );
                coll.data.pop_back();
        }

        {
                auto& collected = coll.data.back();
                assert_test_start( collected, 0, 0 );
                coll.data.pop_back();
        }

        asrt_msg_ctor_test_start( 42, 0, asrt_rec_span_to_span_cb, &sp );
        check_recv_and_spin( &reac, buffer, sp.b, ASRT_FLAG_TSTART );

        CHECK_EQ( 1, data.counter );
        {
                auto& collected = coll.data.back();
                assert_test_result( collected, 0, ASRT_TEST_RESULT_ERROR );
                coll.data.pop_back();
        }

        {
                auto& collected = coll.data.back();
                assert_test_start( collected, 42, 0 );
                coll.data.pop_back();
        }
}

TEST_CASE_FIXTURE( reactor_ctx, "reactor_start_busy" )
{
        check_reactor_init( &reac, send, "rec1" );

        struct asrt_test t1;
        uint64_t         counter = 8;
        setup_test( &reac, &t1, "test1", &counter, &countdown_test );

        asrt_msg_ctor_test_start( 0, 0, asrt_rec_span_to_span_cb, &sp );
        check_reactor_recv_flags( &reac, ( struct asrt_span ){ buffer, sp.b }, ASRT_FLAG_TSTART );

        check_reactor_tick( &reac );
        CHECK_EQ( 8, counter );
        check_reactor_tick( &reac );
        CHECK_EQ( 7, counter );

        {
                auto& collected = coll.data.back();
                assert_test_start( collected, 0, 0 );
                coll.data.pop_back();
        }

        check_reactor_recv_flags( &reac, ( struct asrt_span ){ buffer, sp.b }, ASRT_FLAG_TSTART );

        check_reactor_tick( &reac );
        CHECK_EQ( 7, counter );

        {
                auto& collected = coll.data.back();
                assert_test_result( collected, 0, ASRT_TEST_RESULT_ERROR );
                coll.data.pop_back();
        }

        {
                auto& collected = coll.data.back();
                assert_test_start( collected, 0, 0 );
                coll.data.pop_back();
        }
}

TEST_CASE_FIXTURE( reactor_ctx, "check_macro" )
{
        check_reactor_init( &reac, send, "rec1" );
        struct asrt_test        t1;
        struct asrt_diag_client diag;
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
                assert_test_result( collected, 0, ASRT_TEST_RESULT_FAILURE );
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
        struct asrt_test        t1;
        struct asrt_diag_client diag;
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
                assert_test_result( collected, 0, ASRT_TEST_RESULT_FAILURE );
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

        struct asrt_test t1;
        uint64_t         counter = 0;
        setup_test( &reac, &t1, "test1", &counter, &countdown_test );

        for ( uint32_t x = 0; x < 42; x++ ) {
                counter = 1;
                check_run_test( &reac, 0, x );
                {
                        auto& collected = coll.data.back();
                        assert_test_result( collected, x, ASRT_TEST_RESULT_SUCCESS );
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
        CHECK_EQ( ASRT_SUCCESS, asrt_reactor_init( &reac, send, "desc" ) );
        // set only unknown flag bits (known flags are 0x01..0x20); the else branch must signal an
        // error
        reac.flags          = 0x40;
        enum asrt_status st = asrt_chann_tick( &reac.node, 0 );
        CHECK_NE( ASRT_SUCCESS, st );
}

// R03: duplicate TEST_INFO before tick must be rejected
TEST_CASE_FIXTURE( reactor_ctx, "reactor_test_info_repeat" )
{
        check_reactor_init( &reac, send, "rec1" );

        struct asrt_test t1;
        setup_test( &reac, &t1, "test1", NULL, &dataless_test_fun );

        asrt_msg_ctor_test_info( 0, asrt_rec_span_to_span_cb, &sp );

        // first recv — flag must be set
        check_reactor_recv_flags( &reac, ( struct asrt_span ){ buffer, sp.b }, ASRT_FLAG_TI );

        // second recv before tick — must be rejected
        enum asrt_status st = asrt_chann_recv( &reac.node, ( struct asrt_span ){ buffer, sp.b } );
        CHECK_EQ( ASRT_RECV_UNEXPECTED_ERR, st );

        // flag must still be set
        CHECK_EQ( ASRT_FLAG_TI, reac.flags & ~ASRT_PASSIVE_FLAGS );
}

// R03: duplicate TEST_START before tick must be rejected
TEST_CASE_FIXTURE( reactor_ctx, "reactor_test_start_repeat" )
{
        check_reactor_init( &reac, send, "rec1" );

        struct asrt_test t1;
        setup_test( &reac, &t1, "test1", NULL, &dataless_test_fun );

        asrt_msg_ctor_test_start( 0, 42, asrt_rec_span_to_span_cb, &sp );

        // first recv — flag must be set
        check_reactor_recv_flags( &reac, ( struct asrt_span ){ buffer, sp.b }, ASRT_FLAG_TSTART );

        // second recv before tick — must be rejected
        enum asrt_status st = asrt_chann_recv( &reac.node, ( struct asrt_span ){ buffer, sp.b } );
        CHECK_EQ( ASRT_RECV_UNEXPECTED_ERR, st );

        // flag must still be set
        CHECK_EQ( ASRT_FLAG_TSTART, reac.flags & ~ASRT_PASSIVE_FLAGS );
}

// R04: add_test must be rejected after the first recv call
TEST_CASE_FIXTURE( reactor_ctx, "reactor_add_test_after_recv" )
{
        check_reactor_init( &reac, send, "rec1" );

        struct asrt_test t1, t2;
        setup_test( &reac, &t1, "test1", NULL, &dataless_test_fun );

        // any valid recv locks registration
        asrt_msg_ctor_proto_version( asrt_rec_span_to_span_cb, &sp );
        check_reactor_recv( &reac, ( struct asrt_span ){ buffer, sp.b } );

        // adding a test after recv must be rejected
        enum asrt_status st = asrt_test_init( &t2, "test2", NULL, &dataless_test_fun );
        CHECK_EQ( ASRT_SUCCESS, st );
        st = asrt_reactor_add_test( &reac, &t2 );
        CHECK_EQ( ASRT_BUSY_ERR, st );

        // test list must not have grown
        CHECK_EQ( t1.next, nullptr );
}

// continue_f returning an error sets ASRT_TEST_ERROR → sends ASRT_TEST_ERROR result
TEST_CASE_FIXTURE( reactor_ctx, "reactor_test_error" )
{
        check_reactor_init( &reac, send, "rec1" );

        struct asrt_test t1;
        setup_test( &reac, &t1, "test1", NULL, &error_continue_fun );

        check_run_test( &reac, 0, 0 );

        {
                auto& collected = coll.data.back();
                assert_test_result( collected, 0, ASRT_TEST_RESULT_ERROR );
                coll.data.pop_back();
        }

        {
                auto& collected = coll.data.back();
                assert_test_start( collected, 0, 0 );
                coll.data.pop_back();
        }
}

// two requests received before any tick — both flags set simultaneously
TEST_CASE_FIXTURE( reactor_ctx, "reactor_multi_flag" )
{
        check_reactor_init( &reac, send, "rec1" );

        // First request: test count
        asrt_msg_ctor_test_count( asrt_rec_span_to_span_cb, &sp );
        uint8_t* end1 = sp.b;
        check_reactor_recv( &reac, ( struct asrt_span ){ buffer, end1 } );

        // Second request: description (no tick between)
        sp.b = buffer;
        asrt_msg_ctor_desc( asrt_rec_span_to_span_cb, &sp );
        uint8_t* end2 = sp.b;
        check_reactor_recv( &reac, ( struct asrt_span ){ buffer, end2 } );

        // Both flags must be set at the same time
        CHECK( ( reac.flags & ASRT_FLAG_TC ) );
        CHECK( ( reac.flags & ASRT_FLAG_DESC ) );

        // First tick: DESC handled (highest priority in if-else chain)
        enum asrt_status st = asrt_chann_tick( &reac.node, 0 );
        CHECK_EQ( ASRT_SUCCESS, st );
        {
                auto& collected = coll.data.back();
                assert_collected_core_hdr( collected, 0x06, ASRT_MSG_DESC );
                assert_data_ll_contain_str( "rec1", collected, 2 );
                coll.data.pop_back();
        }
        CHECK( ( reac.flags & ASRT_FLAG_TC ) );
        CHECK( !( reac.flags & ASRT_FLAG_DESC ) );

        // Second tick: TC handled
        st = asrt_chann_tick( &reac.node, 0 );
        CHECK_EQ( ASRT_SUCCESS, st );
        {
                auto& collected = coll.data.back();
                assert_collected_core_hdr( collected, 0x04, ASRT_MSG_TEST_COUNT );
                assert_u16( 0x00, collected.data.data() + 2 );
                coll.data.pop_back();
        }
        CHECK( !( reac.flags & ASRT_FLAG_TC ) );
}

TEST_CASE_FIXTURE( reactor_ctx, "diag_init" )
{
        struct asrt_diag_client diag = {};

        // NULL diag
        CHECK_EQ( ASRT_INIT_ERR, asrt_diag_client_init( NULL, &reac.node, send ) );

        // NULL prev
        CHECK_EQ( ASRT_INIT_ERR, asrt_diag_client_init( &diag, NULL, send ) );

        // valid init
        check_reactor_init( &reac, send, "rec1" );
        CHECK_EQ( ASRT_SUCCESS, asrt_diag_client_init( &diag, &reac.node, send ) );
        CHECK_EQ( ASRT_DIAG, diag.node.chid );
        CHECK_EQ( &diag.node, reac.node.next );  // node appended after prev
        CHECK_EQ( nullptr, diag.node.next );     // chain-terminal
}

TEST_CASE_FIXTURE( reactor_ctx, "diag_record" )
{
        check_reactor_init( &reac, send, "rec1" );
        struct asrt_diag_client diag = {};
        check_diag_init( &diag, &reac.node, send );

        // normal call — verify full byte content
        asrt_diag_client_record( &diag, "test.c", 42, nullptr );
        {
                auto& collected = coll.data.back();
                assert_diag_record( collected, 42 );
                CHECK_EQ( 1u + 4u + 1u + 6u, collected.data.size() );
                assert_data_ll_contain_str( "test.c", collected, 6 );
                coll.data.pop_back();
        }

        // line = 0
        asrt_diag_client_record( &diag, "f.c", 0, nullptr );
        {
                auto& collected = coll.data.back();
                assert_diag_record( collected, 0 );
                coll.data.pop_back();
        }

        // line = UINT32_MAX
        asrt_diag_client_record( &diag, "f.c", UINT32_MAX, nullptr );
        {
                auto& collected = coll.data.back();
                assert_diag_record( collected, UINT32_MAX );
                coll.data.pop_back();
        }
}

// two consecutive CHECK failures → two diag messages, counter incremented twice
TEST_CASE_FIXTURE( reactor_ctx, "check_macro_two_fails" )
{
        check_reactor_init( &reac, send, "rec1" );
        struct asrt_test        t1;
        struct asrt_diag_client diag;
        check_diag_init( &diag, &reac.node, send );
        struct astrt_check_ctx check_ctx = { .diag = &diag, .counter = 0 };
        setup_test( &reac, &t1, "test1", &check_ctx, &check_macro_two_fails );

        check_run_test( &reac, 0, 0 );

        CHECK_EQ( 2, check_ctx.counter );
        {
                auto& collected = coll.data.back();
                assert_test_result( collected, 0, ASRT_TEST_RESULT_FAILURE );
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

// one CHECK failure then one pass → one diag message, counter = 2
TEST_CASE_FIXTURE( reactor_ctx, "check_macro_fail_pass" )
{
        check_reactor_init( &reac, send, "rec1" );
        struct asrt_test        t1;
        struct asrt_diag_client diag;
        check_diag_init( &diag, &reac.node, send );
        struct astrt_check_ctx check_ctx = { .diag = &diag, .counter = 0 };
        setup_test( &reac, &t1, "test1", &check_ctx, &check_macro_fail_pass );

        check_run_test( &reac, 0, 0 );

        CHECK_EQ( 2, check_ctx.counter );
        {
                auto& collected = coll.data.back();
                assert_test_result( collected, 0, ASRT_TEST_RESULT_FAILURE );
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

// failing REQUIRE → code after it unreachable
TEST_CASE_FIXTURE( reactor_ctx, "require_fail_then_check" )
{
        check_reactor_init( &reac, send, "rec1" );
        struct asrt_test        t1;
        struct asrt_diag_client diag;
        check_diag_init( &diag, &reac.node, send );
        struct astrt_check_ctx check_ctx = { .diag = &diag, .counter = 0 };
        setup_test( &reac, &t1, "test1", &check_ctx, &require_then_check );

        check_run_test( &reac, 0, 0 );

        CHECK_EQ( 0, check_ctx.counter );  // counter never incremented
        {
                auto& collected = coll.data.back();
                assert_test_result( collected, 0, ASRT_TEST_RESULT_FAILURE );
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

// CHECK fails, REQUIRE passes, CHECK fails → two diag messages, counter = 3
TEST_CASE_FIXTURE( reactor_ctx, "mix_check_require_check" )
{
        check_reactor_init( &reac, send, "rec1" );
        struct asrt_test        t1;
        struct asrt_diag_client diag;
        check_diag_init( &diag, &reac.node, send );
        struct astrt_check_ctx check_ctx = { .diag = &diag, .counter = 0 };
        setup_test( &reac, &t1, "test1", &check_ctx, &mix_check_require_check );

        check_run_test( &reac, 0, 0 );

        CHECK_EQ( 3, check_ctx.counter );
        {
                auto& collected = coll.data.back();
                assert_test_result( collected, 0, ASRT_TEST_RESULT_FAILURE );
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

// CHECK fails, REQUIRE fails → two diag messages, counter = 1
TEST_CASE_FIXTURE( reactor_ctx, "mix_check_require_fail" )
{
        check_reactor_init( &reac, send, "rec1" );
        struct asrt_test        t1;
        struct asrt_diag_client diag;
        check_diag_init( &diag, &reac.node, send );
        struct astrt_check_ctx check_ctx = { .diag = &diag, .counter = 0 };
        setup_test( &reac, &t1, "test1", &check_ctx, &mix_check_require_fail );

        check_run_test( &reac, 0, 0 );

        CHECK_EQ( 1, check_ctx.counter );  // second increment unreachable
        {
                auto& collected = coll.data.back();
                assert_test_result( collected, 0, ASRT_TEST_RESULT_FAILURE );
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

// truncated and trailing-byte recv errors in asrt_reactor_recv
TEST_CASE_FIXTURE( reactor_ctx, "reactor_recv_truncated" )
{
        check_reactor_init( &reac, send, "rec1" );

        uint8_t          buf[16];
        struct asrt_span sp;
        enum asrt_status rst;

        // Truncated TEST_INFO: only message ID, no u16 tid
        sp = ( struct asrt_span ){ .b = buf, .e = buf + sizeof buf };
        asrt_add_u16( &sp.b, ASRT_MSG_TEST_INFO );
        rst = asrt_chann_recv( &reac.node, ( struct asrt_span ){ .b = buf, .e = sp.b } );
        CHECK_EQ( ASRT_RECV_ERR, rst );

        // Truncated TEST_START: only ID + partial tid(2), missing run_id(4)
        sp = ( struct asrt_span ){ .b = buf, .e = buf + sizeof buf };
        asrt_add_u16( &sp.b, ASRT_MSG_TEST_START );
        asrt_add_u16( &sp.b, 0 );  // tid only, no run_id
        rst = asrt_chann_recv( &reac.node, ( struct asrt_span ){ .b = buf, .e = sp.b } );
        CHECK_EQ( ASRT_RECV_ERR, rst );

        // Trailing bytes: PROTO_VERSION request + extra bytes
        sp = ( struct asrt_span ){ .b = buf, .e = buf + sizeof buf };
        asrt_add_u16( &sp.b, ASRT_MSG_PROTO_VERSION );
        asrt_add_u16( &sp.b, 0xFFFF );  // extra bytes after a no-payload message
        rst = asrt_chann_recv( &reac.node, ( struct asrt_span ){ .b = buf, .e = sp.b } );
        CHECK_EQ( ASRT_RECV_ERR, rst );
}

// ============================================================================
// asrt_param_client — reactor PARAM channel (Phase 3)
// ============================================================================

static inline enum asrt_status call_rtr_param_client_recv(
    struct asrt_param_client* p,
    uint8_t*                  b,
    uint8_t*                  e )
{
        return asrt_chann_recv( &p->node, ( struct asrt_span ){ .b = b, .e = e } );
}

static uint8_t* build_param_ready( uint8_t* buf, asrt::flat_id root_id )
{
        uint8_t* p = buf;
        *p++       = ASRT_PARAM_MSG_READY;
        asrt_add_u32( &p, root_id );
        return p;
}

static uint8_t* build_param_error( uint8_t* buf, uint8_t error_code, asrt::flat_id node_id )
{
        uint8_t* p = buf;
        *p++       = ASRT_PARAM_MSG_ERROR;
        *p++       = error_code;
        asrt_add_u32( &p, node_id );
        return p;
}

// Build a RESPONSE payload: msg_id + nodes + trailing next_sibling_id
// Each node: u32 id | key\0 | u8 type | value bytes
// Common header: RESPONSE msg_id + node_id + key\0
// Returns pointer past the key terminator, ready for type byte + value.
static uint8_t* build_param_response_head( uint8_t* buf, asrt::flat_id node_id, char const* key )
{
        uint8_t* p = buf;
        *p++       = ASRT_PARAM_MSG_RESPONSE;
        asrt_add_u32( &p, node_id );
        size_t klen = strlen( key );
        memcpy( p, key, klen );
        p += klen;
        *p++ = '\0';
        return p;
}

static uint8_t* build_param_response_u32(
    uint8_t*      buf,
    asrt::flat_id node_id,
    char const*   key,
    uint32_t      value,
    asrt::flat_id next_sibling_id )
{
        uint8_t* p = build_param_response_head( buf, node_id, key );
        *p++       = ASRT_FLAT_STYPE_U32;
        asrt_add_u32( &p, value );
        asrt_add_u32( &p, next_sibling_id );
        return p;
}

static uint8_t* build_param_response_i32(
    uint8_t*      buf,
    asrt::flat_id node_id,
    char const*   key,
    int32_t       value,
    asrt::flat_id next_sibling_id )
{
        uint8_t* p = build_param_response_head( buf, node_id, key );
        *p++       = ASRT_FLAT_STYPE_I32;
        asrt_add_i32( &p, value );
        asrt_add_u32( &p, next_sibling_id );
        return p;
}

static uint8_t* build_param_response_str(
    uint8_t*      buf,
    asrt::flat_id node_id,
    char const*   key,
    char const*   value,
    asrt::flat_id next_sibling_id )
{
        uint8_t* p  = build_param_response_head( buf, node_id, key );
        *p++        = ASRT_FLAT_STYPE_STR;
        size_t vlen = strlen( value );
        memcpy( p, value, vlen );
        p += vlen;
        *p++ = '\0';
        asrt_add_u32( &p, next_sibling_id );
        return p;
}

static uint8_t* build_param_response_float(
    uint8_t*      buf,
    asrt::flat_id node_id,
    char const*   key,
    float         value,
    asrt::flat_id next_sibling_id )
{
        uint8_t* p = build_param_response_head( buf, node_id, key );
        *p++       = ASRT_FLAT_STYPE_FLOAT;
        uint32_t bits;
        memcpy( &bits, &value, sizeof bits );
        asrt_add_u32( &p, bits );
        asrt_add_u32( &p, next_sibling_id );
        return p;
}

static uint8_t* build_param_response_bool(
    uint8_t*      buf,
    asrt::flat_id node_id,
    char const*   key,
    uint32_t      value,
    asrt::flat_id next_sibling_id )
{
        uint8_t* p = build_param_response_head( buf, node_id, key );
        *p++       = ASRT_FLAT_STYPE_BOOL;
        asrt_add_u32( &p, value );
        asrt_add_u32( &p, next_sibling_id );
        return p;
}

static uint8_t* build_param_response_obj(
    uint8_t*      buf,
    asrt::flat_id node_id,
    char const*   key,
    asrt::flat_id first_child,
    asrt::flat_id last_child,
    asrt::flat_id next_sibling_id )
{
        uint8_t* p = build_param_response_head( buf, node_id, key );
        *p++       = ASRT_FLAT_CTYPE_OBJECT;
        asrt_add_u32( &p, first_child );
        asrt_add_u32( &p, last_child );
        asrt_add_u32( &p, next_sibling_id );
        return p;
}

static uint8_t* build_param_response_arr(
    uint8_t*      buf,
    asrt::flat_id node_id,
    char const*   key,
    asrt::flat_id first_child,
    asrt::flat_id last_child,
    asrt::flat_id next_sibling_id )
{
        uint8_t* p = build_param_response_head( buf, node_id, key );
        *p++       = ASRT_FLAT_CTYPE_ARRAY;
        asrt_add_u32( &p, first_child );
        asrt_add_u32( &p, last_child );
        asrt_add_u32( &p, next_sibling_id );
        return p;
}

struct param_client_ctx
{
        static constexpr uint32_t BUF_SZ = 256;

        struct asrt_node         head            = {};
        struct asrt_param_client client          = {};
        uint8_t                  msg_buf[BUF_SZ] = {};
        collector                coll;
        asrt_sender              sendr = {};
        struct asrt_param_query  query = {};

        // callback state
        int           resp_called = 0;
        asrt::flat_id resp_id     = 0;
        std::string   resp_key;
        uint32_t      resp_u32_val  = 0;
        uint8_t       resp_type     = 0;
        asrt::flat_id resp_next_sib = 0;
        uint8_t       error_code    = 0;
        asrt::flat_id error_node_id = 0;
        int           error_called  = 0;
        uint32_t      t             = 100;

        static void query_cb(
            struct asrt_param_client*,
            struct asrt_param_query* q,
            struct asrt_flat_value   val )
        {
                auto* ctx = (param_client_ctx*) q->cb_ptr;
                if ( q->error_code != 0 ) {
                        ctx->error_code    = q->error_code;
                        ctx->error_node_id = q->node_id;
                        ctx->error_called++;
                } else {
                        ctx->resp_called++;
                        ctx->resp_id       = q->node_id;
                        ctx->resp_key      = q->key ? q->key : "";
                        ctx->resp_type     = (uint8_t) val.type;
                        ctx->resp_u32_val  = val.data.s.u32_val;
                        ctx->resp_next_sib = q->next_sibling;
                }
        }

        // helpers
        void make_ready( asrt::flat_id root_id = 1u )
        {
                uint8_t buf[8];
                REQUIRE_EQ(
                    ASRT_SUCCESS,
                    call_rtr_param_client_recv( &client, buf, build_param_ready( buf, root_id ) ) );
                REQUIRE_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
                coll.data.clear();
        }

        param_client_ctx()
        {
                head.chid = ASRT_CORE;
                setup_sender_collector( &sendr, &coll );
                struct asrt_span mb = { .b = msg_buf, .e = msg_buf + BUF_SZ };
                REQUIRE_EQ(
                    ASRT_SUCCESS, asrt_param_client_init( &client, &head, sendr, mb, 100 ) );
        }
        ~param_client_ctx() { asrt_param_client_deinit( &client ); }
};

TEST_CASE( "asrt_param_client_init" )
{
        struct asrt_node head            = {};
        head.chid                        = ASRT_CORE;
        asrt_sender              null_s  = {};
        struct asrt_param_client client  = {};
        uint8_t                  buf[64] = {};
        struct asrt_span         mb      = { .b = buf, .e = buf + sizeof buf };
        struct asrt_span         bad     = { .b = nullptr, .e = nullptr };

        CHECK_EQ( ASRT_INIT_ERR, asrt_param_client_init( NULL, &head, null_s, mb, 0 ) );
        CHECK_EQ( ASRT_INIT_ERR, asrt_param_client_init( &client, NULL, null_s, mb, 0 ) );
        CHECK_EQ( ASRT_INIT_ERR, asrt_param_client_init( &client, &head, null_s, bad, 0 ) );
        CHECK_EQ( ASRT_INIT_ERR, asrt_param_client_init( &client, &head, null_s, mb, 0 ) );

        CHECK_EQ( ASRT_SUCCESS, asrt_param_client_init( &client, &head, null_s, mb, 100 ) );
        CHECK_EQ( ASRT_PARA, client.node.chid );
        CHECK_NE( nullptr, (void*) (uintptr_t) client.node.e_cb_ptr );
        CHECK_EQ( &client.node, head.next );
        CHECK_EQ( 0, client.ready );
        asrt_param_client_deinit( &client );
}

TEST_CASE_FIXTURE( param_client_ctx, "asrt_param_client_ready_sends_ack_and_stores_root" )
{
        uint8_t buf[8];
        CHECK_EQ(
            ASRT_SUCCESS,
            call_rtr_param_client_recv( &client, buf, build_param_ready( buf, 3u ) ) );
        CHECK_EQ( 0, client.ready );

        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );

        // READY_ACK sent with capacity as big-endian u32
        REQUIRE_EQ( 1u, coll.data.size() );
        auto& msg = coll.data.front();
        CHECK_EQ( ASRT_PARA, msg.id );
        REQUIRE_EQ( 5u, msg.data.size() );
        CHECK_EQ( ASRT_PARAM_MSG_READY_ACK, msg.data[0] );
        CHECK_EQ( 0u, msg.data[1] );
        CHECK_EQ( 0u, msg.data[2] );
        CHECK_EQ( 1u, msg.data[3] );  // 256 = 0x00000100
        CHECK_EQ( 0u, msg.data[4] );

        CHECK_EQ( 1, client.ready );
        CHECK_EQ( 3u, asrt_param_client_root_id( &client ) );
}

TEST_CASE_FIXTURE( param_client_ctx, "asrt_param_client_query_before_ready_returns_error" )
{
        CHECK_EQ(
            ASRT_ARG_ERR, asrt_param_client_fetch_any( &query, &client, 2u, nullptr, nullptr ) );
        CHECK( coll.data.empty() );
}

TEST_CASE_FIXTURE( param_client_ctx, "asrt_param_client_query_cache_miss_sends_wire" )
{
        make_ready( 1u );

        CHECK_EQ(
            ASRT_SUCCESS, asrt_param_client_fetch_any( &query, &client, 2u, query_cb, this ) );
        // query itself does NOT send — tick does the cache lookup + sends on miss
        CHECK( coll.data.empty() );

        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        REQUIRE_EQ( 1u, coll.data.size() );
        auto& msg = coll.data.front();
        CHECK_EQ( ASRT_PARA, msg.id );
        REQUIRE_EQ( 5u, msg.data.size() );
        CHECK_EQ( ASRT_PARAM_MSG_QUERY, msg.data[0] );
        // node_id = 2 big-endian
        CHECK_EQ( 0u, msg.data[1] );
        CHECK_EQ( 0u, msg.data[2] );
        CHECK_EQ( 0u, msg.data[3] );
        CHECK_EQ( 2u, msg.data[4] );
}

TEST_CASE_FIXTURE( param_client_ctx, "asrt_param_client_response_delivers_one_node" )
{
        make_ready( 1u );

        CHECK_EQ(
            ASRT_SUCCESS, asrt_param_client_fetch_any( &query, &client, 10u, query_cb, this ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );  // cache miss → wire
        coll.data.clear();

        // Inject a RESPONSE with one node: id=10, key="abc", type=U32, value=42, next_sib=0
        uint8_t  rbuf[64];
        uint8_t* re = build_param_response_u32( rbuf, 10u, "abc", 42u, 0u );
        CHECK_EQ( ASRT_SUCCESS, call_rtr_param_client_recv( &client, rbuf, re ) );

        CHECK_EQ( 0, resp_called );  // pending
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        CHECK_EQ( 1, resp_called );
        CHECK_EQ( 10u, resp_id );
        CHECK_EQ( "abc", resp_key );
        CHECK_EQ( ASRT_FLAT_STYPE_U32, resp_type );
        CHECK_EQ( 42u, resp_u32_val );
        CHECK_EQ( 0u, resp_next_sib );

        // query cleared after delivery
        CHECK_EQ( nullptr, client.pending_query );
}

TEST_CASE_FIXTURE( param_client_ctx, "asrt_param_client_cache_hit_delivers_without_wire" )
{
        make_ready( 1u );

        // First query — cache miss, sends wire, gets response
        CHECK_EQ(
            ASRT_SUCCESS, asrt_param_client_fetch_any( &query, &client, 10u, query_cb, this ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );  // wire
        coll.data.clear();

        uint8_t  rbuf[64];
        uint8_t* re = build_param_response_u32( rbuf, 10u, "abc", 42u, 0u );
        CHECK_EQ( ASRT_SUCCESS, call_rtr_param_client_recv( &client, rbuf, re ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );  // delivers
        resp_called = 0;

        // Second query for same node — should be a cache hit, no wire sent
        CHECK_EQ(
            ASRT_SUCCESS, asrt_param_client_fetch_any( &query, &client, 10u, query_cb, this ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );

        CHECK_EQ( 1, resp_called );
        CHECK_EQ( 10u, resp_id );
        CHECK_EQ( "abc", resp_key );
        CHECK_EQ( 42u, resp_u32_val );
        // no wire message sent
        CHECK( coll.data.empty() );
}

TEST_CASE_FIXTURE( param_client_ctx, "asrt_param_client_error_dispatches_and_clears" )
{
        make_ready( 1u );

        CHECK_EQ(
            ASRT_SUCCESS, asrt_param_client_fetch_any( &query, &client, 5u, query_cb, this ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );  // cache miss → wire
        coll.data.clear();

        // Inject PARAM_ERROR
        uint8_t buf[8];
        CHECK_EQ(
            ASRT_SUCCESS,
            call_rtr_param_client_recv(
                &client, buf, build_param_error( buf, ASRT_PARAM_ERR_RESPONSE_TOO_LARGE, 5u ) ) );
        CHECK_EQ( 0, error_called );

        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        CHECK_EQ( 1, error_called );
        CHECK_EQ( ASRT_PARAM_ERR_RESPONSE_TOO_LARGE, error_code );
        CHECK_EQ( 5u, error_node_id );

        // query cleared after error dispatch
        CHECK_EQ( nullptr, client.pending_query );
}

TEST_CASE_FIXTURE( param_client_ctx, "asrt_param_client_cache_next_sibling_stored" )
{
        make_ready( 1u );

        CHECK_EQ(
            ASRT_SUCCESS, asrt_param_client_fetch_any( &query, &client, 10u, query_cb, this ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );  // wire
        coll.data.clear();

        // RESPONSE with next_sibling_id = 99
        uint8_t  rbuf[64];
        uint8_t* re = build_param_response_u32( rbuf, 10u, "x", 7u, 99u );
        CHECK_EQ( ASRT_SUCCESS, call_rtr_param_client_recv( &client, rbuf, re ) );
        CHECK_EQ( 99u, client.cache_next_sibling );

        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        CHECK_EQ( 1, resp_called );
        CHECK_EQ( 99u, resp_next_sib );
}

// ============================================================================
// Typed query API tests
// ============================================================================

// --- query_u32 ---

TEST_CASE_FIXTURE( param_client_ctx, "query_u32_happy" )
{
        make_ready( 1u );
        int      called = 0;
        uint32_t got    = 0;
        struct
        {
                int*      called;
                uint32_t* got;
        } ctx   = { &called, &got };
        auto cb = []( asrt_param_client*, asrt_param_query* q, uint32_t val ) {
                auto* c = (decltype( ctx )*) q->cb_ptr;
                ( *c->called )++;
                *c->got = val;
        };
        CHECK_EQ( ASRT_SUCCESS, asrt_param_client_fetch_u32( &query, &client, 10u, cb, &ctx ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );  // wire
        uint8_t  rbuf[64];
        uint8_t* re = build_param_response_u32( rbuf, 10u, "k", 42u, 0u );
        CHECK_EQ( ASRT_SUCCESS, call_rtr_param_client_recv( &client, rbuf, re ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        CHECK_EQ( 1, called );
        CHECK_EQ( 42u, got );
        CHECK_EQ( 0u, query.error_code );
}

TEST_CASE_FIXTURE( param_client_ctx, "query_u32_type_mismatch" )
{
        make_ready( 1u );
        int called = 0;
        struct
        {
                int* called;
        } ctx   = { &called };
        auto cb = []( asrt_param_client*, asrt_param_query* q, uint32_t ) {
                auto* c = (decltype( ctx )*) q->cb_ptr;
                ( *c->called )++;
        };
        CHECK_EQ( ASRT_SUCCESS, asrt_param_client_fetch_u32( &query, &client, 10u, cb, &ctx ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        uint8_t  rbuf[64];
        uint8_t* re = build_param_response_str( rbuf, 10u, "k", "hello", 0u );
        CHECK_EQ( ASRT_SUCCESS, call_rtr_param_client_recv( &client, rbuf, re ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        CHECK_EQ( 1, called );
        CHECK_EQ( ASRT_PARAM_ERR_TYPE_MISMATCH, query.error_code );
}

// --- query_i32 ---

TEST_CASE_FIXTURE( param_client_ctx, "query_i32_happy" )
{
        make_ready( 1u );
        int     called = 0;
        int32_t got    = 0;
        struct
        {
                int*     called;
                int32_t* got;
        } ctx   = { &called, &got };
        auto cb = []( asrt_param_client*, asrt_param_query* q, int32_t val ) {
                auto* c = (decltype( ctx )*) q->cb_ptr;
                ( *c->called )++;
                *c->got = val;
        };
        CHECK_EQ( ASRT_SUCCESS, asrt_param_client_fetch_i32( &query, &client, 10u, cb, &ctx ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        uint8_t  rbuf[64];
        uint8_t* re = build_param_response_i32( rbuf, 10u, "k", -7, 0u );
        CHECK_EQ( ASRT_SUCCESS, call_rtr_param_client_recv( &client, rbuf, re ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        CHECK_EQ( 1, called );
        CHECK_EQ( -7, got );
        CHECK_EQ( 0u, query.error_code );
}

TEST_CASE_FIXTURE( param_client_ctx, "query_i32_type_mismatch" )
{
        make_ready( 1u );
        int called = 0;
        struct
        {
                int* called;
        } ctx   = { &called };
        auto cb = []( asrt_param_client*, asrt_param_query* q, int32_t ) {
                auto* c = (decltype( ctx )*) q->cb_ptr;
                ( *c->called )++;
        };
        CHECK_EQ( ASRT_SUCCESS, asrt_param_client_fetch_i32( &query, &client, 10u, cb, &ctx ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        uint8_t  rbuf[64];
        uint8_t* re = build_param_response_u32( rbuf, 10u, "k", 99u, 0u );
        CHECK_EQ( ASRT_SUCCESS, call_rtr_param_client_recv( &client, rbuf, re ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        CHECK_EQ( 1, called );
        CHECK_EQ( ASRT_PARAM_ERR_TYPE_MISMATCH, query.error_code );
}

// --- query_str ---

TEST_CASE_FIXTURE( param_client_ctx, "query_str_happy" )
{
        make_ready( 1u );
        int         called = 0;
        std::string got;
        struct
        {
                int*         called;
                std::string* got;
        } ctx   = { &called, &got };
        auto cb = []( asrt_param_client*, asrt_param_query* q, char const* val ) {
                auto* c = (decltype( ctx )*) q->cb_ptr;
                ( *c->called )++;
                if ( val )
                        *c->got = val;
        };
        CHECK_EQ( ASRT_SUCCESS, asrt_param_client_fetch_str( &query, &client, 10u, cb, &ctx ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        uint8_t  rbuf[64];
        uint8_t* re = build_param_response_str( rbuf, 10u, "k", "hello", 0u );
        CHECK_EQ( ASRT_SUCCESS, call_rtr_param_client_recv( &client, rbuf, re ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        CHECK_EQ( 1, called );
        CHECK_EQ( "hello", got );
        CHECK_EQ( 0u, query.error_code );
}

TEST_CASE_FIXTURE( param_client_ctx, "query_str_type_mismatch" )
{
        make_ready( 1u );
        int called = 0;
        struct
        {
                int* called;
        } ctx   = { &called };
        auto cb = []( asrt_param_client*, asrt_param_query* q, char const* ) {
                auto* c = (decltype( ctx )*) q->cb_ptr;
                ( *c->called )++;
        };
        CHECK_EQ( ASRT_SUCCESS, asrt_param_client_fetch_str( &query, &client, 10u, cb, &ctx ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        uint8_t  rbuf[64];
        uint8_t* re = build_param_response_u32( rbuf, 10u, "k", 1u, 0u );
        CHECK_EQ( ASRT_SUCCESS, call_rtr_param_client_recv( &client, rbuf, re ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        CHECK_EQ( 1, called );
        CHECK_EQ( ASRT_PARAM_ERR_TYPE_MISMATCH, query.error_code );
}

// --- query_float ---

TEST_CASE_FIXTURE( param_client_ctx, "query_float_happy" )
{
        make_ready( 1u );
        int   called = 0;
        float got    = 0.0f;
        struct
        {
                int*   called;
                float* got;
        } ctx   = { &called, &got };
        auto cb = []( asrt_param_client*, asrt_param_query* q, float val ) {
                auto* c = (decltype( ctx )*) q->cb_ptr;
                ( *c->called )++;
                *c->got = val;
        };
        CHECK_EQ( ASRT_SUCCESS, asrt_param_client_fetch_float( &query, &client, 10u, cb, &ctx ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        uint8_t  rbuf[64];
        uint8_t* re = build_param_response_float( rbuf, 10u, "k", 3.14f, 0u );
        CHECK_EQ( ASRT_SUCCESS, call_rtr_param_client_recv( &client, rbuf, re ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        CHECK_EQ( 1, called );
        CHECK_EQ( 3.14f, got );
        CHECK_EQ( 0u, query.error_code );
}

TEST_CASE_FIXTURE( param_client_ctx, "query_float_type_mismatch" )
{
        make_ready( 1u );
        int called = 0;
        struct
        {
                int* called;
        } ctx   = { &called };
        auto cb = []( asrt_param_client*, asrt_param_query* q, float ) {
                auto* c = (decltype( ctx )*) q->cb_ptr;
                ( *c->called )++;
        };
        CHECK_EQ( ASRT_SUCCESS, asrt_param_client_fetch_float( &query, &client, 10u, cb, &ctx ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        uint8_t  rbuf[64];
        uint8_t* re = build_param_response_u32( rbuf, 10u, "k", 1u, 0u );
        CHECK_EQ( ASRT_SUCCESS, call_rtr_param_client_recv( &client, rbuf, re ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        CHECK_EQ( 1, called );
        CHECK_EQ( ASRT_PARAM_ERR_TYPE_MISMATCH, query.error_code );
}

// --- query_bool ---

TEST_CASE_FIXTURE( param_client_ctx, "query_bool_happy" )
{
        make_ready( 1u );
        int      called = 0;
        uint32_t got    = 0;
        struct
        {
                int*      called;
                uint32_t* got;
        } ctx   = { &called, &got };
        auto cb = []( asrt_param_client*, asrt_param_query* q, uint32_t val ) {
                auto* c = (decltype( ctx )*) q->cb_ptr;
                ( *c->called )++;
                *c->got = val;
        };
        CHECK_EQ( ASRT_SUCCESS, asrt_param_client_fetch_bool( &query, &client, 10u, cb, &ctx ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        uint8_t  rbuf[64];
        uint8_t* re = build_param_response_bool( rbuf, 10u, "k", 1u, 0u );
        CHECK_EQ( ASRT_SUCCESS, call_rtr_param_client_recv( &client, rbuf, re ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        CHECK_EQ( 1, called );
        CHECK_EQ( 1u, got );
        CHECK_EQ( 0u, query.error_code );
}

TEST_CASE_FIXTURE( param_client_ctx, "query_bool_type_mismatch" )
{
        make_ready( 1u );
        int called = 0;
        struct
        {
                int* called;
        } ctx   = { &called };
        auto cb = []( asrt_param_client*, asrt_param_query* q, uint32_t ) {
                auto* c = (decltype( ctx )*) q->cb_ptr;
                ( *c->called )++;
        };
        CHECK_EQ( ASRT_SUCCESS, asrt_param_client_fetch_bool( &query, &client, 10u, cb, &ctx ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        uint8_t  rbuf[64];
        uint8_t* re = build_param_response_str( rbuf, 10u, "k", "nope", 0u );
        CHECK_EQ( ASRT_SUCCESS, call_rtr_param_client_recv( &client, rbuf, re ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        CHECK_EQ( 1, called );
        CHECK_EQ( ASRT_PARAM_ERR_TYPE_MISMATCH, query.error_code );
}

// --- query_obj ---

TEST_CASE_FIXTURE( param_client_ctx, "query_obj_happy" )
{
        make_ready( 1u );
        int                  called = 0;
        asrt_flat_child_list got    = {};
        struct
        {
                int*                  called;
                asrt_flat_child_list* got;
        } ctx   = { &called, &got };
        auto cb = []( asrt_param_client*, asrt_param_query* q, asrt_flat_child_list val ) {
                auto* c = (decltype( ctx )*) q->cb_ptr;
                ( *c->called )++;
                *c->got = val;
        };
        CHECK_EQ( ASRT_SUCCESS, asrt_param_client_fetch_obj( &query, &client, 10u, cb, &ctx ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        uint8_t  rbuf[64];
        uint8_t* re = build_param_response_obj( rbuf, 10u, "k", 2u, 5u, 0u );
        CHECK_EQ( ASRT_SUCCESS, call_rtr_param_client_recv( &client, rbuf, re ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        CHECK_EQ( 1, called );
        CHECK_EQ( 2u, got.first_child );
        CHECK_EQ( 5u, got.last_child );
        CHECK_EQ( 0u, query.error_code );
}

TEST_CASE_FIXTURE( param_client_ctx, "query_obj_type_mismatch" )
{
        make_ready( 1u );
        int called = 0;
        struct
        {
                int* called;
        } ctx   = { &called };
        auto cb = []( asrt_param_client*, asrt_param_query* q, asrt_flat_child_list ) {
                auto* c = (decltype( ctx )*) q->cb_ptr;
                ( *c->called )++;
        };
        CHECK_EQ( ASRT_SUCCESS, asrt_param_client_fetch_obj( &query, &client, 10u, cb, &ctx ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        uint8_t  rbuf[64];
        uint8_t* re = build_param_response_u32( rbuf, 10u, "k", 1u, 0u );
        CHECK_EQ( ASRT_SUCCESS, call_rtr_param_client_recv( &client, rbuf, re ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        CHECK_EQ( 1, called );
        CHECK_EQ( ASRT_PARAM_ERR_TYPE_MISMATCH, query.error_code );
}

// --- query_arr ---

TEST_CASE_FIXTURE( param_client_ctx, "query_arr_happy" )
{
        make_ready( 1u );
        int                  called = 0;
        asrt_flat_child_list got    = {};
        struct
        {
                int*                  called;
                asrt_flat_child_list* got;
        } ctx   = { &called, &got };
        auto cb = []( asrt_param_client*, asrt_param_query* q, asrt_flat_child_list val ) {
                auto* c = (decltype( ctx )*) q->cb_ptr;
                ( *c->called )++;
                *c->got = val;
        };
        CHECK_EQ( ASRT_SUCCESS, asrt_param_client_fetch_arr( &query, &client, 10u, cb, &ctx ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        uint8_t  rbuf[64];
        uint8_t* re = build_param_response_arr( rbuf, 10u, "k", 3u, 6u, 0u );
        CHECK_EQ( ASRT_SUCCESS, call_rtr_param_client_recv( &client, rbuf, re ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        CHECK_EQ( 1, called );
        CHECK_EQ( 3u, got.first_child );
        CHECK_EQ( 6u, got.last_child );
        CHECK_EQ( 0u, query.error_code );
}

TEST_CASE_FIXTURE( param_client_ctx, "query_arr_type_mismatch" )
{
        make_ready( 1u );
        int called = 0;
        struct
        {
                int* called;
        } ctx   = { &called };
        auto cb = []( asrt_param_client*, asrt_param_query* q, asrt_flat_child_list ) {
                auto* c = (decltype( ctx )*) q->cb_ptr;
                ( *c->called )++;
        };
        CHECK_EQ( ASRT_SUCCESS, asrt_param_client_fetch_arr( &query, &client, 10u, cb, &ctx ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        uint8_t  rbuf[64];
        uint8_t* re = build_param_response_u32( rbuf, 10u, "k", 1u, 0u );
        CHECK_EQ( ASRT_SUCCESS, call_rtr_param_client_recv( &client, rbuf, re ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        CHECK_EQ( 1, called );
        CHECK_EQ( ASRT_PARAM_ERR_TYPE_MISMATCH, query.error_code );
}

// --- typed query: server error delivers null val ---

TEST_CASE_FIXTURE( param_client_ctx, "query_u32_server_error_delivers_null" )
{
        make_ready( 1u );
        int called = 0;
        struct
        {
                int* called;
        } ctx   = { &called };
        auto cb = []( asrt_param_client*, asrt_param_query* q, uint32_t ) {
                auto* c = (decltype( ctx )*) q->cb_ptr;
                ( *c->called )++;
        };
        CHECK_EQ( ASRT_SUCCESS, asrt_param_client_fetch_u32( &query, &client, 5u, cb, &ctx ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        uint8_t buf[8];
        CHECK_EQ(
            ASRT_SUCCESS,
            call_rtr_param_client_recv(
                &client, buf, build_param_error( buf, ASRT_PARAM_ERR_RESPONSE_TOO_LARGE, 5u ) ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        CHECK_EQ( 1, called );
        CHECK_EQ( ASRT_PARAM_ERR_RESPONSE_TOO_LARGE, query.error_code );
}

// --- pending query guard ---

TEST_CASE_FIXTURE( param_client_ctx, "query_rejects_when_pending" )
{
        make_ready( 1u );
        CHECK_EQ(
            ASRT_SUCCESS, asrt_param_client_fetch_any( &query, &client, 10u, query_cb, this ) );
        // second query while first is pending
        asrt_param_query query2 = {};
        CHECK_EQ(
            ASRT_ARG_ERR, asrt_param_client_fetch_any( &query2, &client, 11u, query_cb, this ) );
}

// --- query pending flag ---

TEST_CASE_FIXTURE( param_client_ctx, "query_pending_flag" )
{
        make_ready( 1u );
        CHECK_FALSE( asrt_param_query_pending( &client ) );

        CHECK_EQ(
            ASRT_SUCCESS, asrt_param_client_fetch_any( &query, &client, 10u, query_cb, this ) );
        CHECK( asrt_param_query_pending( &client ) );

        // deliver response
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
        coll.data.clear();
        uint8_t  rbuf[64];
        uint8_t* re = build_param_response_u32( rbuf, 10u, "k", 42u, 0u );
        CHECK_EQ( ASRT_SUCCESS, call_rtr_param_client_recv( &client, rbuf, re ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );

        CHECK_FALSE( asrt_param_query_pending( &client ) );
        CHECK_EQ( 1, resp_called );
}

// ============================================================================
// Query timeout tests
// ============================================================================

struct param_timeout_ctx
{
        static constexpr uint32_t BUF_SZ  = 256;
        static constexpr uint32_t TIMEOUT = 10;

        struct asrt_node         head            = {};
        struct asrt_param_client client          = {};
        uint8_t                  msg_buf[BUF_SZ] = {};
        collector                coll;
        asrt_sender              sendr = {};
        struct asrt_param_query  query = {};

        int           cb_called  = 0;
        uint8_t       error_code = 0;
        asrt::flat_id resp_id    = 0;
        uint32_t      resp_u32   = 0;
        uint32_t      t          = 1;

        static void query_cb(
            struct asrt_param_client*,
            struct asrt_param_query* q,
            struct asrt_flat_value   val )
        {
                auto* ctx = (param_timeout_ctx*) q->cb_ptr;
                ctx->cb_called++;
                ctx->error_code = q->error_code;
                ctx->resp_id    = q->node_id;
                ctx->resp_u32   = val.data.s.u32_val;
        }

        void make_ready( asrt::flat_id root_id = 1u )
        {
                uint8_t buf[8];
                REQUIRE_EQ(
                    ASRT_SUCCESS,
                    call_rtr_param_client_recv( &client, buf, build_param_ready( buf, root_id ) ) );
                REQUIRE_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, t++ ) );
                coll.data.clear();
        }

        param_timeout_ctx()
        {
                head.chid = ASRT_CORE;
                setup_sender_collector( &sendr, &coll );
                struct asrt_span mb = { .b = msg_buf, .e = msg_buf + BUF_SZ };
                REQUIRE_EQ(
                    ASRT_SUCCESS, asrt_param_client_init( &client, &head, sendr, mb, TIMEOUT ) );
        }
        ~param_timeout_ctx() { asrt_param_client_deinit( &client ); }
};

TEST_CASE_FIXTURE( param_timeout_ctx, "query_timeout_fires_after_deadline" )
{
        make_ready( 1u );

        CHECK_EQ(
            ASRT_SUCCESS, asrt_param_client_fetch_any( &query, &client, 10u, query_cb, this ) );
        // first tick: DELIVER → cache miss → sends wire query
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 100 ) );
        CHECK_EQ( 0, cb_called );

        // second tick: NONE with pending_query → starts timing (query_start = 105)
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 105 ) );
        CHECK_EQ( 0, cb_called );

        // tick just before timeout (105 + 10 - 1 = 114): no timeout
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 114 ) );
        CHECK_EQ( 0, cb_called );

        // tick at timeout (115 - 105 = 10 >= TIMEOUT): fires
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 115 ) );
        CHECK_EQ( 1, cb_called );
        CHECK_EQ( ASRT_PARAM_ERR_TIMEOUT, error_code );
        CHECK_FALSE( asrt_param_query_pending( &client ) );
}

TEST_CASE_FIXTURE( param_timeout_ctx, "query_no_timeout_when_response_arrives" )
{
        make_ready( 1u );

        CHECK_EQ(
            ASRT_SUCCESS, asrt_param_client_fetch_any( &query, &client, 10u, query_cb, this ) );
        // DELIVER tick → cache miss → wire
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 100 ) );

        // start timing
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 102 ) );

        // response arrives before timeout
        uint8_t  rbuf[64];
        uint8_t* re = build_param_response_u32( rbuf, 10u, "k", 42u, 0u );
        CHECK_EQ( ASRT_SUCCESS, call_rtr_param_client_recv( &client, rbuf, re ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 105 ) );

        CHECK_EQ( 1, cb_called );
        CHECK_EQ( 0u, error_code );
        CHECK_EQ( 42u, resp_u32 );
}

TEST_CASE_FIXTURE( param_timeout_ctx, "query_timeout_resets_for_new_query" )
{
        make_ready( 1u );

        // First query — let it timeout
        CHECK_EQ(
            ASRT_SUCCESS, asrt_param_client_fetch_any( &query, &client, 10u, query_cb, this ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 100 ) );  // DELIVER → wire
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 100 ) );  // start timing
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 200 ) );  // timeout
        CHECK_EQ( 1, cb_called );
        CHECK_EQ( ASRT_PARAM_ERR_TIMEOUT, error_code );

        // Reset state
        cb_called  = 0;
        error_code = 0;

        // Second query — should start fresh timing
        CHECK_EQ(
            ASRT_SUCCESS, asrt_param_client_fetch_any( &query, &client, 20u, query_cb, this ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 300 ) );  // DELIVER → wire
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 300 ) );  // start timing
        // not timed out yet
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 305 ) );
        CHECK_EQ( 0, cb_called );

        // now timeout
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 310 ) );
        CHECK_EQ( 1, cb_called );
        CHECK_EQ( ASRT_PARAM_ERR_TIMEOUT, error_code );
}

TEST_CASE_FIXTURE( param_timeout_ctx, "query_timeout_with_typed_callback" )
{
        make_ready( 1u );

        int      called = 0;
        uint32_t got    = 99;
        struct
        {
                int*      called;
                uint32_t* got;
                uint8_t   err;
        } ctx   = { &called, &got, 0 };
        auto cb = []( asrt_param_client*, asrt_param_query* q, uint32_t val ) {
                auto* c = (decltype( ctx )*) q->cb_ptr;
                ( *c->called )++;
                *c->got = val;
                c->err  = q->error_code;
        };

        CHECK_EQ( ASRT_SUCCESS, asrt_param_client_fetch_u32( &query, &client, 10u, cb, &ctx ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 50 ) );  // wire
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 50 ) );  // start timing
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 60 ) );  // timeout
        CHECK_EQ( 1, called );
        CHECK_EQ( ASRT_PARAM_ERR_TIMEOUT, ctx.err );
        CHECK_EQ( 0u, got );  // zero value on timeout
}

// ============================================================================
// asrt_collect_client — reactor COLL channel
// ============================================================================

static inline enum asrt_status call_collect_client_recv(
    struct asrt_collect_client* c,
    uint8_t*                    b,
    uint8_t*                    e )
{
        return asrt_chann_recv( &c->node, ( struct asrt_span ){ .b = b, .e = e } );
}

static uint8_t* build_coll_ready(
    uint8_t*      buf,
    asrt::flat_id root_id,
    asrt::flat_id next_node_id = 1 )
{
        uint8_t* p = buf;
        *p++       = ASRT_COLLECT_MSG_READY;
        asrt_add_u32( &p, root_id );
        asrt_add_u32( &p, next_node_id );
        return p;
}

static uint8_t* build_coll_error( uint8_t* buf, uint8_t error_code )
{
        uint8_t* p = buf;
        *p++       = ASRT_COLLECT_MSG_ERROR;
        *p++       = error_code;
        return p;
}

struct collect_client_ctx
{
        struct asrt_node           head   = {};
        struct asrt_collect_client client = {};
        collector                  coll;
        asrt_sender                sendr = {};

        void make_active( asrt::flat_id root_id = 1u )
        {
                uint8_t buf[16];
                REQUIRE_EQ(
                    ASRT_SUCCESS,
                    call_collect_client_recv( &client, buf, build_coll_ready( buf, root_id ) ) );
                REQUIRE_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
                coll.data.clear();
        }

        collect_client_ctx()
        {
                head.chid = ASRT_CORE;
                setup_sender_collector( &sendr, &coll );
                REQUIRE_EQ( ASRT_SUCCESS, asrt_collect_client_init( &client, &head, sendr ) );
        }
        ~collect_client_ctx() = default;
};

TEST_CASE( "asrt_collect_client_init" )
{
        struct asrt_node head             = {};
        head.chid                         = ASRT_CORE;
        asrt_sender                null_s = {};
        struct asrt_collect_client c      = {};

        CHECK_EQ( ASRT_INIT_ERR, asrt_collect_client_init( NULL, &head, null_s ) );
        CHECK_EQ( ASRT_INIT_ERR, asrt_collect_client_init( &c, NULL, null_s ) );

        CHECK_EQ( ASRT_SUCCESS, asrt_collect_client_init( &c, &head, null_s ) );
        CHECK_EQ( ASRT_COLL, c.node.chid );
        CHECK_NE( nullptr, (void*) (uintptr_t) c.node.e_cb_ptr );
        CHECK_EQ( &c.node, head.next );
        CHECK_EQ( ASRT_COLLECT_CLIENT_IDLE, c.state );
}

TEST_CASE_FIXTURE( collect_client_ctx, "asrt_collect_client_ready_handshake" )
{
        uint8_t buf[16];
        CHECK_EQ(
            ASRT_SUCCESS, call_collect_client_recv( &client, buf, build_coll_ready( buf, 42u ) ) );
        CHECK_EQ( ASRT_COLLECT_CLIENT_READY_RECV, client.state );
        CHECK_EQ( 42u, client.root_id );

        // tick sends READY_ACK
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        CHECK_EQ( ASRT_COLLECT_CLIENT_ACTIVE, client.state );
        CHECK_EQ( 42u, asrt_collect_client_root_id( &client ) );

        // Verify READY_ACK was sent
        REQUIRE_EQ( 1u, coll.data.size() );
        auto& msg = coll.data.front();
        CHECK_EQ( ASRT_COLL, msg.id );
        REQUIRE_EQ( 1u, msg.data.size() );
        CHECK_EQ( ASRT_COLLECT_MSG_READY_ACK, msg.data[0] );
}

TEST_CASE_FIXTURE( collect_client_ctx, "asrt_collect_client_append_sends_wire" )
{
        make_active( 1u );

        CHECK_EQ( ASRT_SUCCESS, asrt_collect_client_append_u32( &client, 0, "alpha", 99 ) );

        REQUIRE_EQ( 1u, coll.data.size() );
        auto& msg = coll.data.front();
        CHECK_EQ( ASRT_COLL, msg.id );
        // msg_id(1) + parent_id(4) + node_id(4) + "alpha\0"(6) + type(1) + u32(4) = 20
        REQUIRE_EQ( 20u, msg.data.size() );
        CHECK_EQ( ASRT_COLLECT_MSG_APPEND, msg.data[0] );
        assert_u32( 0u, msg.data.data() + 1 );  // parent_id
        assert_u32( 1u, msg.data.data() + 5 );  // node_id (auto-assigned)
        CHECK_EQ( std::string( "alpha" ), std::string( (char*) &msg.data[9] ) );
}

TEST_CASE_FIXTURE( collect_client_ctx, "asrt_collect_client_append_before_active_returns_error" )
{
        CHECK_EQ( ASRT_ARG_ERR, asrt_collect_client_append_u32( &client, 0, NULL, 1 ) );
}

TEST_CASE_FIXTURE( collect_client_ctx, "asrt_collect_client_error_sets_fatal" )
{
        make_active( 1u );

        uint8_t buf[4];
        CHECK_EQ(
            ASRT_SUCCESS, call_collect_client_recv( &client, buf, build_coll_error( buf, 0x01 ) ) );
        CHECK_EQ( ASRT_COLLECT_CLIENT_ERROR, client.state );
}

TEST_CASE_FIXTURE( collect_client_ctx, "asrt_collect_client_append_after_error_returns_error" )
{
        make_active( 1u );

        // Inject error
        uint8_t buf[4];
        call_collect_client_recv( &client, buf, build_coll_error( buf, 0x01 ) );

        CHECK_EQ( ASRT_ARG_ERR, asrt_collect_client_append_u32( &client, 0, NULL, 1 ) );
}

TEST_CASE_FIXTURE( collect_client_ctx, "asrt_collect_client_ready_from_error_re_handshakes" )
{
        make_active( 1u );

        // Inject error
        uint8_t buf[16];
        call_collect_client_recv( &client, buf, build_coll_error( buf, 0x01 ) );
        CHECK_EQ( ASRT_COLLECT_CLIENT_ERROR, client.state );

        // READY accepted even from ERROR state
        CHECK_EQ(
            ASRT_SUCCESS, call_collect_client_recv( &client, buf, build_coll_ready( buf, 99u ) ) );
        CHECK_EQ( ASRT_COLLECT_CLIENT_READY_RECV, client.state );

        // Complete handshake
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        CHECK_EQ( ASRT_COLLECT_CLIENT_ACTIVE, client.state );
        CHECK_EQ( 99u, asrt_collect_client_root_id( &client ) );

        // Append works again, node_id counter starts from next_node_id
        coll.data.clear();
        CHECK_EQ( ASRT_SUCCESS, asrt_collect_client_append_u32( &client, 0, "x", 7 ) );
        REQUIRE_EQ( 1u, coll.data.size() );
        assert_u32( 1u, coll.data.front().data.data() + 5 );  // node_id starts from 1
}

TEST_CASE_FIXTURE( collect_client_ctx, "asrt_collect_client_ready_re_handshake" )
{
        make_active( 1u );

        // Second READY resets to handshake
        uint8_t buf[16];
        CHECK_EQ(
            ASRT_SUCCESS, call_collect_client_recv( &client, buf, build_coll_ready( buf, 7u ) ) );
        CHECK_EQ( ASRT_COLLECT_CLIENT_READY_RECV, client.state );

        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        CHECK_EQ( ASRT_COLLECT_CLIENT_ACTIVE, client.state );
        CHECK_EQ( 7u, asrt_collect_client_root_id( &client ) );
}

TEST_CASE_FIXTURE( collect_client_ctx, "asrt_collect_client_tick_idle_is_noop" )
{
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        CHECK_EQ( ASRT_COLLECT_CLIENT_IDLE, client.state );
        CHECK( coll.data.empty() );
}

// =====================================================================
// stream client tests
// =====================================================================

#include "../asrtr/stream.h"

/// Sender that collects messages and optionally fires done_cb synchronously.
struct strm_sender_ctx
{
        collector* coll       = nullptr;
        bool       fail_send  = false;
        bool       sync_done  = true;   ///< call done_cb synchronously (like loopback)
        bool       defer_done = false;  ///< don't call done_cb at all (async)

        // Stash for deferred done_cb firing
        asrt_send_done_cb stashed_cb  = nullptr;
        void*             stashed_ptr = nullptr;
};

static enum asrt_status strm_sender_cb(
    void*                 data,
    asrt_chann_id         id,
    struct asrt_rec_span* buff,
    asrt_send_done_cb     done_cb,
    void*                 done_ptr )
{
        auto* ctx = static_cast< strm_sender_ctx* >( data );
        if ( ctx->fail_send )
                return ASRT_SEND_ERR;

        // Collect the data
        if ( ctx->coll )
                sender_collect( ctx->coll, id, buff, nullptr, nullptr );

        if ( ctx->defer_done ) {
                ctx->stashed_cb  = done_cb;
                ctx->stashed_ptr = done_ptr;
        } else if ( ctx->sync_done && done_cb ) {
                done_cb( done_ptr, ASRT_SUCCESS );
        }
        return ASRT_SUCCESS;
}

/// fire previously deferred done_cb
static void strm_fire_deferred( strm_sender_ctx* ctx, enum asrt_status status )
{
        REQUIRE( ctx->stashed_cb );
        ctx->stashed_cb( ctx->stashed_ptr, status );
        ctx->stashed_cb  = nullptr;
        ctx->stashed_ptr = nullptr;
}

struct strm_client_ctx
{
        collector          coll;
        strm_sender_ctx    sctx{ .coll = &coll, .sync_done = true, .defer_done = false };
        asrt_sender        sender{ .ptr = &sctx, .cb = strm_sender_cb };
        asrt_node          root{};
        asrt_stream_client client = {};

        strm_client_ctx()
        {
                CHECK_EQ( ASRT_SUCCESS, asrt_stream_client_init( &client, &root, sender ) );
        }
        ~strm_client_ctx() { coll.data.clear(); }
};

// --- init ---

TEST_CASE( "strm_client_init: null client" )
{
        asrt_node   root   = {};
        asrt_sender sender = {};
        CHECK_EQ( ASRT_INIT_ERR, asrt_stream_client_init( nullptr, &root, sender ) );
}

TEST_CASE( "strm_client_init: null prev" )
{
        asrt_stream_client client = {};
        asrt_sender        sender = {};
        CHECK_EQ( ASRT_INIT_ERR, asrt_stream_client_init( &client, nullptr, sender ) );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_init: valid" )
{
        CHECK_EQ( ASRT_STRM_IDLE, client.state );
        CHECK_EQ( ASRT_STRM, client.node.chid );
        CHECK_EQ( &client.node, root.next );
}

// --- define ---

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_define: null client" )
{
        enum asrt_strm_field_type_e fields[] = { ASRT_STRM_FIELD_U8 };
        CHECK_EQ(
            ASRT_ARG_ERR, asrt_stream_client_define( nullptr, 1, fields, 1, nullptr, nullptr ) );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_define: null fields" )
{
        CHECK_EQ(
            ASRT_ARG_ERR, asrt_stream_client_define( &client, 1, nullptr, 1, nullptr, nullptr ) );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_define: zero field_count" )
{
        enum asrt_strm_field_type_e fields[] = { ASRT_STRM_FIELD_U8 };
        CHECK_EQ(
            ASRT_ARG_ERR, asrt_stream_client_define( &client, 1, fields, 0, nullptr, nullptr ) );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_define: valid sets DEFINE_SEND" )
{
        enum asrt_strm_field_type_e fields[] = { ASRT_STRM_FIELD_U8, ASRT_STRM_FIELD_U16 };
        CHECK_EQ(
            ASRT_SUCCESS, asrt_stream_client_define( &client, 5, fields, 2, nullptr, nullptr ) );
        CHECK_EQ( ASRT_STRM_DEFINE_SEND, client.state );
        CHECK_EQ( 5, client.op.define.schema_id );
        CHECK_EQ( 2, client.op.define.field_count );
        CHECK_EQ( fields, client.op.define.fields );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_define: stores done_cb" )
{
        bool                called = false;
        asrt_stream_done_cb cb     = []( void* p, enum asrt_status ) {
                *static_cast< bool* >( p ) = true;
        };
        enum asrt_strm_field_type_e fields[] = { ASRT_STRM_FIELD_U8 };
        CHECK_EQ( ASRT_SUCCESS, asrt_stream_client_define( &client, 0, fields, 1, cb, &called ) );
        CHECK_EQ( cb, client.done_cb );
        CHECK_EQ( &called, client.done_cb_ptr );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_define: not idle returns BUSY_ERR" )
{
        enum asrt_strm_field_type_e fields[] = { ASRT_STRM_FIELD_U8 };
        CHECK_EQ(
            ASRT_SUCCESS, asrt_stream_client_define( &client, 0, fields, 1, nullptr, nullptr ) );
        // Now in DEFINE_SEND, second define should fail
        CHECK_EQ(
            ASRT_BUSY_ERR, asrt_stream_client_define( &client, 1, fields, 1, nullptr, nullptr ) );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_define: BUSY for each non-idle state" )
{
        enum asrt_strm_field_type_e fields[] = { ASRT_STRM_FIELD_U8 };

        SUBCASE( "DEFINE_SEND" )
        {
                client.state = ASRT_STRM_DEFINE_SEND;
        }
        SUBCASE( "WAIT" )
        {
                client.state = ASRT_STRM_WAIT;
        }
        SUBCASE( "DONE" )
        {
                client.state = ASRT_STRM_DONE;
        }
        SUBCASE( "ERROR" )
        {
                client.state = ASRT_STRM_ERROR;
        }

        CHECK_EQ(
            ASRT_BUSY_ERR, asrt_stream_client_define( &client, 0, fields, 1, nullptr, nullptr ) );
}

// --- tick ---

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_tick: idle is noop" )
{
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        CHECK_EQ( ASRT_STRM_IDLE, client.state );
        CHECK( coll.data.empty() );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_tick: WAIT is noop" )
{
        client.state = ASRT_STRM_WAIT;
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        CHECK_EQ( ASRT_STRM_WAIT, client.state );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_tick: ERROR is noop" )
{
        client.state = ASRT_STRM_ERROR;
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        CHECK_EQ( ASRT_STRM_ERROR, client.state );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_tick: DEFINE_SEND sends and sync-completes" )
{
        enum asrt_strm_field_type_e fields[]   = { ASRT_STRM_FIELD_U32, ASRT_STRM_FIELD_I8 };
        bool                        done_fired = false;
        asrt_status                 done_st    = ASRT_SIZE_ERR;
        auto                        done_cb    = []( void* p, enum asrt_status s ) {
                auto* pair = static_cast< std::pair< bool*, asrt_status* >* >( p );
                *pair->first  = true;
                *pair->second = s;
        };
        auto pair = std::make_pair( &done_fired, &done_st );
        CHECK_EQ(
            ASRT_SUCCESS, asrt_stream_client_define( &client, 3, fields, 2, done_cb, &pair ) );

        // sync_done = true (default), so tick sends define and done_cb fires —
        // but completion is never handled inline, so state is DONE after first tick
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        CHECK_EQ( ASRT_STRM_DONE, client.state );
        CHECK( !done_fired );

        // Second tick fires the done_cb
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        CHECK_EQ( ASRT_STRM_IDLE, client.state );
        CHECK( done_fired );
        CHECK_EQ( ASRT_SUCCESS, done_st );
        // One message should have been sent
        REQUIRE_EQ( 1u, coll.data.size() );
        CHECK_EQ( ASRT_STRM, coll.data[0].id );
        coll.data.clear();
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_tick: DEFINE_SEND with deferred done" )
{
        sctx.sync_done                       = false;
        sctx.defer_done                      = true;
        enum asrt_strm_field_type_e fields[] = { ASRT_STRM_FIELD_U8 };
        CHECK_EQ(
            ASRT_SUCCESS, asrt_stream_client_define( &client, 0, fields, 1, nullptr, nullptr ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        // Should be in WAIT, done hasn't fired yet
        CHECK_EQ( ASRT_STRM_WAIT, client.state );

        // Fire the deferred done
        strm_fire_deferred( &sctx, ASRT_SUCCESS );
        CHECK_EQ( ASRT_STRM_DONE, client.state );

        // Tick should fire the (NULL) done_cb and return to IDLE
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        CHECK_EQ( ASRT_STRM_IDLE, client.state );
        coll.data.clear();
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_tick: DEFINE_SEND send failure" )
{
        sctx.fail_send                       = true;
        enum asrt_strm_field_type_e fields[] = { ASRT_STRM_FIELD_U8 };
        CHECK_EQ(
            ASRT_SUCCESS, asrt_stream_client_define( &client, 0, fields, 1, nullptr, nullptr ) );
        auto st = asrt_chann_tick( &client.node, 0 );
        CHECK_EQ( ASRT_SEND_ERR, st );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_tick: DONE fires user callback" )
{
        sctx.sync_done                       = false;
        sctx.defer_done                      = true;
        enum asrt_strm_field_type_e fields[] = { ASRT_STRM_FIELD_U8 };
        bool                        cb_fired = false;
        asrt_status                 cb_st    = {};
        auto                        done_cb  = []( void* p, enum asrt_status s ) {
                auto* pair = static_cast< std::pair< bool*, asrt_status* >* >( p );
                *pair->first  = true;
                *pair->second = s;
        };
        auto pair = std::make_pair( &cb_fired, &cb_st );
        CHECK_EQ(
            ASRT_SUCCESS, asrt_stream_client_define( &client, 0, fields, 1, done_cb, &pair ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        CHECK_EQ( ASRT_STRM_WAIT, client.state );

        strm_fire_deferred( &sctx, ASRT_SUCCESS );
        CHECK_EQ( ASRT_STRM_DONE, client.state );
        CHECK( !cb_fired );

        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        CHECK( cb_fired );
        CHECK_EQ( ASRT_SUCCESS, cb_st );
        CHECK_EQ( ASRT_STRM_IDLE, client.state );
        coll.data.clear();
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_tick: DONE with NULL callback" )
{
        sctx.sync_done                       = false;
        sctx.defer_done                      = true;
        enum asrt_strm_field_type_e fields[] = { ASRT_STRM_FIELD_U8 };
        CHECK_EQ(
            ASRT_SUCCESS, asrt_stream_client_define( &client, 0, fields, 1, nullptr, nullptr ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        strm_fire_deferred( &sctx, ASRT_SUCCESS );
        CHECK_EQ( ASRT_STRM_DONE, client.state );

        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        CHECK_EQ( ASRT_STRM_IDLE, client.state );
        coll.data.clear();
}

// --- record ---

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_emit: null client" )
{
        uint8_t data[] = { 1 };
        CHECK_EQ( ASRT_ARG_ERR, asrt_stream_client_emit( nullptr, 0, data, 1, nullptr, nullptr ) );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_emit: null data with nonzero size" )
{
        CHECK_EQ(
            ASRT_ARG_ERR, asrt_stream_client_emit( &client, 0, nullptr, 5, nullptr, nullptr ) );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_emit: null data with zero size is error" )
{
        CHECK_EQ(
            ASRT_ARG_ERR, asrt_stream_client_emit( &client, 0, nullptr, 0, nullptr, nullptr ) );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_emit: error state" )
{
        client.state   = ASRT_STRM_ERROR;
        uint8_t data[] = { 1 };
        CHECK_EQ(
            ASRT_INTERNAL_ERR, asrt_stream_client_emit( &client, 0, data, 1, nullptr, nullptr ) );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_emit: send failure" )
{
        sctx.fail_send = true;
        uint8_t data[] = { 1 };
        CHECK_EQ( ASRT_SEND_ERR, asrt_stream_client_emit( &client, 0, data, 1, nullptr, nullptr ) );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_emit: valid sends DATA message" )
{
        uint8_t data[] = { 0xAA, 0xBB };
        CHECK_EQ( ASRT_SUCCESS, asrt_stream_client_emit( &client, 7, data, 2, nullptr, nullptr ) );
        // send_done fires synchronously in test fixture → state is DONE, tick() needed
        CHECK_EQ( ASRT_STRM_DONE, client.state );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        CHECK_EQ( ASRT_STRM_IDLE, client.state );
        REQUIRE_EQ( 1u, coll.data.size() );
        CHECK_EQ( ASRT_STRM, coll.data[0].id );
        REQUIRE_EQ( 4u, coll.data[0].data.size() );
        CHECK_EQ( ASRT_STRM_MSG_DATA, coll.data[0].data[0] );
        CHECK_EQ( 7, coll.data[0].data[1] );
        CHECK_EQ( 0xAA, coll.data[0].data[2] );
        CHECK_EQ( 0xBB, coll.data[0].data[3] );
        coll.data.clear();
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_emit: done_cb fires via tick" )
{
        asrt_status cb_status = {};
        auto        cb        = []( void* p, enum asrt_status s ) {
                *static_cast< asrt_status* >( p ) = s;
        };
        uint8_t data[] = { 1 };
        CHECK_EQ( ASRT_SUCCESS, asrt_stream_client_emit( &client, 0, data, 1, cb, &cb_status ) );
        CHECK_EQ( ASRT_STRM_DONE, client.state );

        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        CHECK_EQ( ASRT_SUCCESS, cb_status );
        CHECK_EQ( ASRT_STRM_IDLE, client.state );
        CHECK_EQ( nullptr, client.done_cb );
        CHECK_EQ( nullptr, client.done_cb_ptr );
        coll.data.clear();
}

// --- recv ---

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_recv: empty buffer is noop" )
{
        uint8_t          buf[1];
        struct asrt_span sp = { .b = buf, .e = buf };
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_recv( &client.node, sp ) );
        CHECK_EQ( ASRT_STRM_IDLE, client.state );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_recv: error message sets ERROR state" )
{
        uint8_t          buf[] = { ASRT_STRM_MSG_ERROR, ASRT_STRM_ERR_UNKNOWN_SCHEMA };
        struct asrt_span sp    = { .b = buf, .e = buf + 2 };
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_recv( &client.node, sp ) );
        CHECK_EQ( ASRT_STRM_ERROR, client.state );
        CHECK_EQ( ASRT_STRM_ERR_UNKNOWN_SCHEMA, client.err_code );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_recv: error message too short" )
{
        uint8_t          buf[] = { ASRT_STRM_MSG_ERROR };
        struct asrt_span sp    = { .b = buf, .e = buf + 1 };
        CHECK_EQ( ASRT_RECV_ERR, asrt_chann_recv( &client.node, sp ) );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_recv: unknown message id" )
{
        uint8_t          buf[] = { 0xFF };
        struct asrt_span sp    = { .b = buf, .e = buf + 1 };
        CHECK_EQ( ASRT_RECV_UNEXPECTED_ERR, asrt_chann_recv( &client.node, sp ) );
}

// --- send_done preserves ERROR ---

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client: send_done preserves ERROR state" )
{
        sctx.sync_done                       = false;
        sctx.defer_done                      = true;
        enum asrt_strm_field_type_e fields[] = { ASRT_STRM_FIELD_U8 };
        CHECK_EQ(
            ASRT_SUCCESS, asrt_stream_client_define( &client, 0, fields, 1, nullptr, nullptr ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        CHECK_EQ( ASRT_STRM_WAIT, client.state );

        // Simulate controller error arriving before send completes
        uint8_t          err_buf[] = { ASRT_STRM_MSG_ERROR, ASRT_STRM_ERR_INVALID_DEFINE };
        struct asrt_span sp        = { .b = err_buf, .e = err_buf + 2 };
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_recv( &client.node, sp ) );
        CHECK_EQ( ASRT_STRM_ERROR, client.state );

        // Now send completes — should NOT overwrite ERROR
        strm_fire_deferred( &sctx, ASRT_SUCCESS );
        CHECK_EQ( ASRT_STRM_ERROR, client.state );
        coll.data.clear();
}

// --- reset ---

TEST_CASE( "strm_client_reset: null client" )
{
        CHECK_EQ( ASRT_INIT_ERR, asrt_stream_client_reset( nullptr ) );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_reset: IDLE" )
{
        CHECK_EQ( ASRT_SUCCESS, asrt_stream_client_reset( &client ) );
        CHECK_EQ( ASRT_STRM_IDLE, client.state );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_reset: DEFINE_SEND returns BUSY" )
{
        enum asrt_strm_field_type_e fields[] = { ASRT_STRM_FIELD_U8 };
        asrt_stream_client_define( &client, 0, fields, 1, nullptr, nullptr );
        CHECK_EQ( ASRT_BUSY_ERR, asrt_stream_client_reset( &client ) );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_reset: WAIT returns BUSY" )
{
        client.state = ASRT_STRM_WAIT;
        CHECK_EQ( ASRT_BUSY_ERR, asrt_stream_client_reset( &client ) );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_reset: DONE resets to IDLE" )
{
        client.state = ASRT_STRM_DONE;
        CHECK_EQ( ASRT_SUCCESS, asrt_stream_client_reset( &client ) );
        CHECK_EQ( ASRT_STRM_IDLE, client.state );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_reset: ERROR resets to IDLE" )
{
        client.state    = ASRT_STRM_ERROR;
        client.err_code = ASRT_STRM_ERR_UNKNOWN_SCHEMA;
        CHECK_EQ( ASRT_SUCCESS, asrt_stream_client_reset( &client ) );
        CHECK_EQ( ASRT_STRM_IDLE, client.state );
        CHECK_EQ( ASRT_STRM_ERR_SUCCESS, client.err_code );
}

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client_reset: clears done_cb" )
{
        client.state       = ASRT_STRM_DONE;
        client.done_cb     = []( void*, enum asrt_status ) {};
        client.done_cb_ptr = (void*) 0xBEEF;
        CHECK_EQ( ASRT_SUCCESS, asrt_stream_client_reset( &client ) );
        CHECK_EQ( nullptr, client.done_cb );
        CHECK_EQ( nullptr, client.done_cb_ptr );
}

// --- full define→tick→idle cycle ---

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client: full define→tick cycle with callback" )
{
        asrt_status cb_status = {};
        auto        cb        = []( void* p, enum asrt_status s ) {
                *static_cast< asrt_status* >( p ) = s;
        };
        enum asrt_strm_field_type_e fields[] = { ASRT_STRM_FIELD_U16 };
        CHECK_EQ(
            ASRT_SUCCESS, asrt_stream_client_define( &client, 2, fields, 1, cb, &cb_status ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        CHECK_EQ( ASRT_STRM_DONE, client.state );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        CHECK_EQ( ASRT_STRM_IDLE, client.state );
        CHECK_EQ( ASRT_SUCCESS, cb_status );

        // Verify define message payload
        REQUIRE_EQ( 1u, coll.data.size() );
        auto& msg = coll.data[0].data;
        REQUIRE_GE( msg.size(), 4u );
        CHECK_EQ( ASRT_STRM_MSG_DEFINE, msg[0] );
        CHECK_EQ( 2, msg[1] );
        CHECK_EQ( 1, msg[2] );
        CHECK_EQ( ASRT_STRM_FIELD_U16, msg[3] );
        coll.data.clear();
}

// --- define then record ---

TEST_CASE_FIXTURE( strm_client_ctx, "strm_client: define then record" )
{
        enum asrt_strm_field_type_e fields[] = { ASRT_STRM_FIELD_U8 };
        CHECK_EQ(
            ASRT_SUCCESS, asrt_stream_client_define( &client, 1, fields, 1, nullptr, nullptr ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        CHECK_EQ( ASRT_STRM_DONE, client.state );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        CHECK_EQ( ASRT_STRM_IDLE, client.state );
        coll.data.clear();

        uint8_t rec_data[] = { 42 };
        CHECK_EQ(
            ASRT_SUCCESS, asrt_stream_client_emit( &client, 1, rec_data, 1, nullptr, nullptr ) );
        CHECK_EQ( ASRT_STRM_DONE, client.state );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &client.node, 0 ) );
        CHECK_EQ( ASRT_STRM_IDLE, client.state );
        REQUIRE_EQ( 1u, coll.data.size() );
        CHECK_EQ( ASRT_STRM_MSG_DATA, coll.data[0].data[0] );
        CHECK_EQ( 1, coll.data[0].data[1] );
        CHECK_EQ( 42, coll.data[0].data[2] );
        coll.data.clear();
}
