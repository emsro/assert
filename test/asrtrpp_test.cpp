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
#include "../asrtl/log.h"
#include "../asrtr/reactor.h"
#include "../asrtr/record.h"
#include "../asrtrpp/diag.hpp"
#include "../asrtrpp/fmt.hpp"
#include "../asrtrpp/reactor.hpp"
#include "./collector.hpp"
#include "./util.h"

#include <algorithm>
#include <doctest/doctest.h>
#include <format>

ASRTL_DEFINE_GPOS_LOG()

// ---------------------------------------------------------------------------
// helpers

struct collect_sender
{
        collector* coll;

        asrtl_status operator()( asrtl_chann_id id, asrtl_rec_span* buff ) const
        {
                return sender_collect( coll, id, buff );
        }
};

void assert_diag_record( collected_data& cd, uint32_t line )
{
        assert_collected_diag_hdr( cd, ASRTL_DIAG_MSG_RECORD );
        assert_u32( line, cd.data.data() + 1 );
        CHECK( cd.data.size() > 5 );
        auto const* fn_b = cd.data.data() + 5;
        auto const* fn_e = cd.data.data() + cd.data.size();
        CHECK( std::none_of( fn_b, fn_e, []( uint8_t b ) {
                return b == '\0';
        } ) );
}

// ---------------------------------------------------------------------------
// test callables for unit<T>

struct pass_test
{
        char const* name() const
        {
                return "pass_test";
        }
        asrtr::status operator()( asrtr::record& rec )
        {
                rec.state = ASRTR_TEST_PASS;
                return ASRTR_SUCCESS;
        }
};

// Returns a transport error — tests the trampoline's error→FAIL mapping.
struct err_cb_test
{
        char const* name() const
        {
                return "err_cb_test";
        }
        asrtr::status operator()( asrtr::record& )
        {
                return ASRTR_INTERNAL_ERR;
        }
};

// Properly-failing test: sets state=FAIL and returns SUCCESS.
struct fail_test
{
        char const* name() const
        {
                return "fail_test";
        }
        asrtr::status operator()( asrtr::record& rec )
        {
                rec.state = ASRTR_TEST_FAIL;
                return ASRTR_SUCCESS;
        }
};

// ---------------------------------------------------------------------------
// fixtures

struct reactor_ctx
{
        collector      coll;
        collect_sender send_fn{ &coll };
        asrtr::reactor r{ send_fn, "test_reactor" };

        ~reactor_ctx()
        {
                CHECK( coll.data.empty() );
        }
};

struct diag_ctx
{
        collector      coll_r;
        collector      coll_d;
        collect_sender send_fn_r{ &coll_r };
        collect_sender send_fn_d{ &coll_d };
        asrtr::reactor r{ send_fn_r, "test_reactor" };
        asrtr::diag    d{ r.node(), send_fn_d };

        ~diag_ctx()
        {
                CHECK( coll_r.data.empty() );
                CHECK( coll_d.data.empty() );
        }
};

// ---------------------------------------------------------------------------
// fmt

TEST_CASE( "fmt_success" )
{
        std::string s = std::format( "{}", ASRTR_SUCCESS );
        CHECK_EQ( s, asrtr_status_to_str( ASRTR_SUCCESS ) );
}

TEST_CASE( "fmt_error" )
{
        std::string s = std::format( "{}", ASRTR_INIT_ERR );
        CHECK_EQ( s, asrtr_status_to_str( ASRTR_INIT_ERR ) );
}

// ---------------------------------------------------------------------------
// reactor

TEST_CASE_FIXTURE( reactor_ctx, "reactor_init" )
{
        CHECK_NE( nullptr, r.node() );
        CHECK_EQ( ASRTL_CORE, r.node()->chid );
}

TEST_CASE_FIXTURE( reactor_ctx, "reactor_tick" )
{
        CHECK_EQ( ASRTR_SUCCESS, r.tick() );
}

// ---------------------------------------------------------------------------
// unit<T>

TEST_CASE( "unit_cb_pass" )
{
        asrtr::unit< pass_test > u;

        asrtr_test_input input{};
        input.test_ptr = &u;

        asrtr_record rec{};
        rec.state = ASRTR_TEST_RUNNING;
        rec.inpt  = &input;

        asrtr::status st = asrtr::unit< pass_test >::cb( &rec );
        CHECK_EQ( ASRTR_SUCCESS, st );
        CHECK_NE( ASRTR_TEST_FAIL, rec.state );
}

TEST_CASE( "unit_cb_fail" )
{
        // When T returns a transport error, cb forces rec.state to FAIL.
        asrtr::unit< err_cb_test > u;

        asrtr_test_input input{};
        input.test_ptr = &u;

        asrtr_record rec{};
        rec.state = ASRTR_TEST_RUNNING;
        rec.inpt  = &input;

        asrtr::status st = asrtr::unit< err_cb_test >::cb( &rec );
        CHECK_NE( ASRTR_SUCCESS, st );
        CHECK_EQ( ASRTR_TEST_FAIL, rec.state );
}

// ---------------------------------------------------------------------------
// diag

TEST_CASE_FIXTURE( diag_ctx, "diag_init" )
{
        CHECK_NE( nullptr, d.node() );
        CHECK_EQ( ASRTL_DIAG, d.node()->chid );
        CHECK_EQ( d.node(), r.node()->next );
}

TEST_CASE_FIXTURE( diag_ctx, "diag_record" )
{
        d.record( "diag_test.cpp", 42 );
        REQUIRE_EQ( 1u, coll_d.data.size() );
        assert_diag_record( coll_d.data.front(), 42 );
        coll_d.data.pop_front();
}

// ---------------------------------------------------------------------------
// reactor end-to-end

void assert_test_start_msg( collected_data& cd, uint16_t test_id, uint32_t run_id )
{
        assert_collected_core_hdr( cd, 0x08, ASRTL_MSG_TEST_START );
        assert_u16( test_id, cd.data.data() + 2 );
        assert_u32( run_id, cd.data.data() + 4 );
}

void assert_test_result_msg( collected_data& cd, uint32_t run_id, enum asrtl_test_result_e result )
{
        assert_collected_core_hdr( cd, 0x08, ASRTL_MSG_TEST_RESULT );
        assert_u32( run_id, cd.data.data() + 2 );
        assert_u16( result, cd.data.data() + 6 );
}

struct e2e_ctx
{
        collector      coll;
        collect_sender send_fn{ &coll };
        asrtr::reactor r{ send_fn, "e2e_reactor" };

        asrtr::unit< pass_test > t0;
        asrtr::unit< fail_test > t1;
        asrtr::unit< pass_test > t2;

        e2e_ctx()
        {
                r.add_test( t0 );
                r.add_test( t1 );
                r.add_test( t2 );
        }

        ~e2e_ctx()
        {
                CHECK( coll.data.empty() );
        }

        void run( uint16_t test_id, uint32_t run_id )
        {
                uint8_t    buf[64];
                asrtl_span sp{ buf, buf + sizeof buf };
                CHECK_EQ(
                    ASRTL_SUCCESS,
                    asrtl_msg_ctor_test_start( test_id, run_id, asrtl_rec_span_to_span_cb, &sp ) );
                CHECK_EQ( ASRTL_SUCCESS, r.node()->recv_cb( r.node()->recv_ptr, { buf, sp.b } ) );
                for ( int i = 0; i < 10; i++ )
                        CHECK_EQ( ASRTR_SUCCESS, r.tick() );
        }
};

TEST_CASE_FIXTURE( e2e_ctx, "reactor_e2e" )
{
        // test 0: pass_test
        run( 0, 10 );
        REQUIRE_EQ( 2u, coll.data.size() );
        assert_test_result_msg( coll.data.back(), 10, ASRTL_TEST_SUCCESS );
        coll.data.pop_back();
        assert_test_start_msg( coll.data.back(), 0, 10 );
        coll.data.pop_back();

        // test 1: fail_test
        run( 1, 20 );
        REQUIRE_EQ( 2u, coll.data.size() );
        assert_test_result_msg( coll.data.back(), 20, ASRTL_TEST_FAILURE );
        coll.data.pop_back();
        assert_test_start_msg( coll.data.back(), 1, 20 );
        coll.data.pop_back();

        // test 2: pass_test (same callable as t0, different slot)
        run( 2, 30 );
        REQUIRE_EQ( 2u, coll.data.size() );
        assert_test_result_msg( coll.data.back(), 30, ASRTL_TEST_SUCCESS );
        coll.data.pop_back();
        assert_test_start_msg( coll.data.back(), 2, 30 );
        coll.data.pop_back();
}
