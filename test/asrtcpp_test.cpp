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
#include "../asrtc/result.h"
#include "../asrtc/status_to_str.h"
#include "../asrtcpp/controller.hpp"
#include "../asrtcpp/diag.hpp"
#include "../asrtcpp/fmt.hpp"
#include "../asrtcpp/param.hpp"
#include "../asrtl/log.h"
#include "../asrtlpp/util.hpp"
#include "../asrtrpp/diag.hpp"
#include "../asrtrpp/reactor.hpp"
#include "./collector.hpp"

#include <doctest/doctest.h>
#include <format>
#include <string>
#include <vector>

ASRTL_DEFINE_GPOS_LOG()

// ---------------------------------------------------------------------------
// helpers

static std::vector< uint8_t > flatten( asrtl::rec_span const* buff )
{
        std::vector< uint8_t > v;
        for ( auto const* seg = buff; seg; seg = seg->next )
                v.insert( v.end(), seg->b, seg->e );
        return v;
}

// ---------------------------------------------------------------------------
// test callables for the reactor side

struct passing_test
{
        char const* name() const
        {
                return "passing_test";
        }
        asrtr::status operator()( asrtr::record& rec )
        {
                rec.state = ASRTR_TEST_PASS;
                return ASRTR_SUCCESS;
        }
};

struct failing_test
{
        char const* name() const
        {
                return "failing_test";
        }
        asrtr::status operator()( asrtr::record& rec )
        {
                rec.state = ASRTR_TEST_FAIL;
                return ASRTR_SUCCESS;
        }
};

// ---------------------------------------------------------------------------
// paired fixture: asrtc::controller <-> asrtr::reactor wired in-process
//
// Member declaration order governs construction:
//   counters / status first, then the optional controller (starts empty),
//   then the reactor unit stubs, then the reactor itself.
// The constructor body emplaces the controller and completes the handshake.

struct paired_ctx
{
        int           init_cb_count = 0;
        asrtc::status init_status   = ASRTC_SUCCESS;
        uint32_t      t             = 1;

        asrtl::opt< asrtc::controller > c;

        asrtr::unit< passing_test > t0;
        asrtr::unit< failing_test > t1;

        // Must be declared before `r` so it is constructed (and valid) before the reactor
        // captures &r_send as its sender pointer.  asrtlpp/sender.hpp stores &CB, so the
        // lambda object must outlive the reactor.
        std::function< asrtl_status( asrtl_chann_id, asrtl_rec_span* ) > r_send;

        // Stable send callable for the controller; must outlive `c`.
        std::function< asrtl_status( asrtl_chann_id, asrtl_rec_span* ) > c_send{
            [this]( asrtl_chann_id, asrtl_rec_span* buff ) {
                    auto  flat = flatten( buff );
                    auto  sp   = asrtl::cnv( std::span{ flat } );
                    auto* rn   = r.node();
                    rn->recv_cb( rn->recv_ptr, sp );
                    return ASRTL_SUCCESS;
            } };

        asrtr::reactor r;

        paired_ctx()
          : r_send( [this]( asrtl_chann_id, asrtl_rec_span* buff ) {
                  auto  flat = flatten( buff );
                  auto  sp   = asrtl::cnv( std::span{ flat } );
                  auto* cn   = ( *c ).node();
                  cn->recv_cb( cn->recv_ptr, sp );
                  return ASRTL_SUCCESS;
          } )
          , r( r_send, "paired_reactor" )
        {
                r.add_test( t0 );
                r.add_test( t1 );

                c.emplace(
                    // controller sends → flatten + forward to reactor's recv_cb
                    c_send,
                    // error callback
                    []( asrtl::source, asrtl::ecode ) -> asrtc::status {
                            return ASRTC_SUCCESS;
                    } );

                // start init handshake
                (void) c->start(
                    // init callback — single-use via cimpl_do
                    [this]( asrtc::status s ) -> asrtc::status {
                            init_cb_count++;
                            init_status = s;
                            return ASRTC_SUCCESS;
                    },
                    1000 );

                // complete PROTO_VERSION handshake
                for ( int i = 0; i < 100 && !c->is_idle(); i++ ) {
                        (void) c->tick( t++ );
                        r.tick();
                }
                CHECK( c->is_idle() );
        }

        // tick both sides until controller is idle again
        void spin()
        {
                for ( int i = 0; i < 100 && !c->is_idle(); i++ ) {
                        (void) c->tick( t++ );
                        r.tick();
                }
                CHECK( c->is_idle() );
        }
};

// ---------------------------------------------------------------------------
// fmt

TEST_CASE( "fmt_success" )
{
        std::string s = std::format( "{}", ASRTC_SUCCESS );
        CHECK_EQ( s, asrtc_status_to_str( ASRTC_SUCCESS ) );
}

TEST_CASE( "fmt_error" )
{
        std::string s = std::format( "{}", ASRTC_CNTR_INIT_ERR );
        CHECK_EQ( s, asrtc_status_to_str( ASRTC_CNTR_INIT_ERR ) );
}

// ---------------------------------------------------------------------------
// controller init

TEST_CASE_FIXTURE( paired_ctx, "controller_init" )
{
        CHECK( c->is_idle() );
        CHECK_NE( nullptr, c->node() );
        CHECK_EQ( ASRTL_CORE, c->node()->chid );
        CHECK_EQ( ASRTC_SUCCESS, init_status );
}

TEST_CASE_FIXTURE( paired_ctx, "init_cb_fires_once" )
{
        // cimpl_do clears init_cb after first call; verify it was called exactly once
        CHECK_EQ( 1, init_cb_count );
}

// ---------------------------------------------------------------------------
// query_desc

TEST_CASE_FIXTURE( paired_ctx, "query_desc" )
{
        std::string   received;
        asrtc::status st = c->query_desc( [&]( asrtc::status s, std::string_view sv ) {
                CHECK_EQ( ASRTC_SUCCESS, s );
                received = std::string{ sv };
                return ASRTC_SUCCESS;
        }, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        spin();
        CHECK_EQ( "paired_reactor", received );
}

// ---------------------------------------------------------------------------
// query_test_count

TEST_CASE_FIXTURE( paired_ctx, "query_test_count" )
{
        // tc_cb receives uint32_t; the C layer produces uint16_t (silent widening in
        // cimpl_test_count)
        uint32_t      count = 0;
        asrtc::status st    = c->query_test_count( [&]( asrtc::status s, uint32_t n ) {
                CHECK_EQ( ASRTC_SUCCESS, s );
                count = n;
                return ASRTC_SUCCESS;
        }, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        spin();
        CHECK_EQ( 2u, count );
}

// ---------------------------------------------------------------------------
// query_test_info

TEST_CASE_FIXTURE( paired_ctx, "query_test_info" )
{
        uint16_t      tid = 0xFFFF;
        std::string   name;
        asrtc::status st =
            c->query_test_info( 0, [&]( asrtc::status s, uint16_t t, std::string_view sv ) {
                    CHECK_EQ( ASRTC_SUCCESS, s );
                    tid  = t;
                    name = std::string{ sv };
                    return ASRTC_SUCCESS;
            }, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        spin();
        CHECK_EQ( 0u, tid );
        CHECK_EQ( "passing_test", name );
}

// ---------------------------------------------------------------------------
// exec_test

TEST_CASE_FIXTURE( paired_ctx, "exec_test_pass" )
{
        asrtc_test_result res = ASRTC_TEST_UNKNOWN;
        asrtc::status     st  = c->exec_test( 0, [&]( asrtc::status s, asrtc::result const& r ) {
                CHECK_EQ( ASRTC_SUCCESS, s );
                res = r.res;
                return ASRTC_SUCCESS;
        }, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        spin();
        CHECK_EQ( ASRTC_TEST_SUCCESS, res );
}

TEST_CASE_FIXTURE( paired_ctx, "exec_test_fail" )
{
        asrtc_test_result res = ASRTC_TEST_UNKNOWN;
        asrtc::status     st  = c->exec_test( 1, [&]( asrtc::status s, asrtc::result const& r ) {
                CHECK_EQ( ASRTC_SUCCESS, s );
                res = r.res;
                return ASRTC_SUCCESS;
        }, 1000 );
        CHECK_EQ( ASRTC_SUCCESS, st );
        spin();
        CHECK_EQ( ASRTC_TEST_FAILURE, res );
}

// ---------------------------------------------------------------------------
// busy_error: second query while controller is busy must fail and NOT store its callback

TEST_CASE_FIXTURE( paired_ctx, "busy_error" )
{
        bool first_called  = false;
        bool second_called = false;

        // start the first query — succeeds, callback stored
        CHECK_EQ( ASRTC_SUCCESS, c->query_desc( [&]( asrtc::status, std::string_view ) {
                first_called = true;
                return ASRTC_SUCCESS;
        }, 1000 ) );

        // controller is now busy — second query must be rejected; its callback must NOT be stored
        CHECK_EQ( ASRTC_CNTR_BUSY_ERR, c->query_test_count( [&]( asrtc::status, uint32_t ) {
                second_called = true;
                return ASRTC_SUCCESS;
        }, 1000 ) );

        // complete the first query
        spin();

        CHECK( first_called );
        CHECK_FALSE( second_called );
}

// ---------------------------------------------------------------------------
// controller + diagnostics
//
// Extends paired_ctx with an asrtr::diag on the reactor side and an
// asrtc::diag on the controller side, wired bidirectionally.
//
// The in-process paired_ctx routes messages directly via recv_cb, bypassing
// channel dispatch.  Each diag sender delivers straight to the peer's
// recv_cb.  Construction order: send lambdas first (capture this, only called
// at runtime), then the diag objects that consume them.

struct diag_paired_ctx : paired_ctx
{
        // c_diag sends → r_diag (controller-to-reactor direction)
        std::function< asrtl_status( asrtl_chann_id, asrtl_rec_span* ) > c_diag_send{
            [this]( asrtl_chann_id, asrtl_rec_span* buff ) {
                    auto  flat = flatten( buff );
                    auto  sp   = asrtl::cnv( std::span{ flat } );
                    auto* rn   = r_diag.node();
                    rn->recv_cb( rn->recv_ptr, sp );
                    return ASRTL_SUCCESS;
            } };

        asrtc::diag c_diag{ ( *c ).node(), c_diag_send };

        // r_diag sends → c_diag (reactor-to-controller direction)
        std::function< asrtl_status( asrtl_chann_id, asrtl_rec_span* ) > r_diag_send{
            [this]( asrtl_chann_id, asrtl_rec_span* buff ) {
                    auto  flat = flatten( buff );
                    auto  sp   = asrtl::cnv( std::span{ flat } );
                    auto* rn   = c_diag.node();
                    rn->recv_cb( rn->recv_ptr, sp );
                    return ASRTL_SUCCESS;
            } };

        asrtr::diag r_diag{ r.node(), r_diag_send };
};

TEST_CASE_FIXTURE( diag_paired_ctx, "diag_record_received_by_controller" )
{
        r_diag.record( "test_file.c", 42 );

        auto rec = c_diag.take_record();
        REQUIRE( rec != nullptr );
        CHECK_EQ( 42u, rec->line );
        CHECK( std::string_view{ rec->file }.ends_with( "test_file.c" ) );

        // no more records
        CHECK( c_diag.take_record() == nullptr );
}

TEST_CASE_FIXTURE( diag_paired_ctx, "diag_multiple_records_queued_in_order" )
{
        r_diag.record( "a.c", 1 );
        r_diag.record( "b.c", 2 );
        r_diag.record( "c.c", 3 );

        auto r1 = c_diag.take_record();
        auto r2 = c_diag.take_record();
        auto r3 = c_diag.take_record();
        REQUIRE( r1 );
        REQUIRE( r2 );
        REQUIRE( r3 );

        CHECK_EQ( 1u, r1->line );
        CHECK_EQ( 2u, r2->line );
        CHECK_EQ( 3u, r3->line );

        CHECK( c_diag.take_record() == nullptr );
}

TEST_CASE_FIXTURE( diag_paired_ctx, "diag_independent_of_controller_queries" )
{
        // both a controller query and a diag record in flight at the same time
        std::string desc;
        CHECK_EQ( ASRTC_SUCCESS, c->query_desc( [&]( asrtc::status s, std::string_view sv ) {
                CHECK_EQ( ASRTC_SUCCESS, s );
                desc = std::string{ sv };
                return ASRTC_SUCCESS;
        }, 1000 ) );

        r_diag.record( "diag_test.c", 99 );

        spin();

        CHECK_EQ( "paired_reactor", desc );

        auto rec = c_diag.take_record();
        REQUIRE( rec != nullptr );
        CHECK_EQ( 99u, rec->line );
}

// ---------------------------------------------------------------------------
// param_server wrapper

struct param_server_ctx : paired_ctx
{
        collector param_coll;

        std::function< asrtl_status( asrtl_chann_id, asrtl_rec_span* ) > param_send{
            [this]( asrtl_chann_id id, asrtl_rec_span* buff ) {
                    return sender_collect( &param_coll, id, buff );
            } };

        asrtc::param_server srv{ ( *c ).node(), param_send, asrtl_default_allocator() };
};

TEST_CASE_FIXTURE( param_server_ctx, "param_server_node_chained" )
{
        CHECK_NE( nullptr, srv.node() );
        CHECK_EQ( ASRTL_PARAM, srv.node()->chid );
}

TEST_CASE_FIXTURE( param_server_ctx, "param_server_set_tree_and_send_ready" )
{
        struct asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "k", asrtl_flat_value_u32( 42 ) );

        srv.set_tree( &tree );
        auto noop = [] {};
        CHECK_EQ( ASRTL_SUCCESS, srv.send_ready( 1u, noop, 1000 ) );

        REQUIRE_EQ( 1u, param_coll.data.size() );
        CHECK_EQ( ASRTL_PARAM, param_coll.data.front().id );
        CHECK_EQ( ASRTL_PARAM_MSG_READY, param_coll.data.front().data[0] );
        param_coll.data.pop_front();

        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_server_ctx, "param_server_ready_ack_cb_fires" )
{
        struct asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );

        srv.set_tree( &tree );

        int  ack_count = 0;
        auto on_ack    = [&] { ++ack_count; };
        CHECK_EQ( ASRTL_SUCCESS, srv.send_ready( 1u, on_ack, 1000 ) );
        param_coll.data.clear();

        // Build a READY_ACK message: [ASRTL_PARAM_MSG_READY_ACK, max_msg_size(256) LE]
        uint8_t    ack_msg[5] = { ASRTL_PARAM_MSG_READY_ACK, 0, 1, 0, 0 };  // 256 LE
        asrtl_span sp         = { .b = ack_msg, .e = ack_msg + sizeof ack_msg };

        auto* n = srv.node();
        n->recv_cb( n->recv_ptr, sp );

        CHECK_EQ( 0, ack_count );  // not yet — pending, needs tick
        CHECK_EQ( ASRTL_SUCCESS, srv.tick( t++ ) );
        CHECK_EQ( 1, ack_count );  // fired once

        // Second tick should not fire again
        CHECK_EQ( ASRTL_SUCCESS, srv.tick( t++ ) );
        CHECK_EQ( 1, ack_count );

        asrtl_flat_tree_deinit( &tree );
}
