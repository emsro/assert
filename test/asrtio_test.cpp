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
#include "../asrtcpp/controller.hpp"
#include "../asrtio/cntr_tcp_sys.hpp"
#include "../asrtio/rsim.hpp"
#include "../asrtio/util.hpp"
#include "../asrtl/chann.h"
#include "../asrtl/cobs.h"
#include "../asrtl/flat_tree.h"
#include "../asrtl/log.h"
#include "../asrtl/util.h"
#include "../asrtlpp/util.hpp"
#include "../asrtrpp/reactor.hpp"

#include <chrono>
#include <doctest/doctest.h>
#include <map>
#include <span>

ASRTL_DEFINE_GPOS_LOG()

// ---------------------------------------------------------------------------
// utest_sim helpers

static asrtio::utest_sim make_sim(
    int              duration_ms,
    int              variance_ms,
    asrtr_test_state res,
    bool             randomize,
    std::mt19937&    rng )
{
        asrtio::utest_sim s;
        s.duration_ms = duration_ms;
        s.variance_ms = variance_ms;
        s.res         = res;
        s.randomize   = randomize;
        s.tname       = "test";
        s.rng_ptr     = &rng;
        return s;
}

// Run sim until it no longer returns ASRTR_TEST_RUNNING (or bail after limit).
static asrtr_test_state run_to_completion( asrtio::utest_sim& sim, int limit = 100'000'000 )
{
        asrtr::record r{};
        for ( int i = 0; i < limit; ++i ) {
                REQUIRE( sim( r ) == ASRTR_SUCCESS );
                if ( r.state != ASRTR_TEST_RUNNING )
                        return r.state;
                // busy-wait — fine for sub-second durations in a test
        }
        return r.state;
}

// ---------------------------------------------------------------------------
// Component 1: utest_sim

TEST_CASE( "utest_sim_insta_pass" )
{
        std::mt19937      rng( 0 );
        asrtio::utest_sim sim = make_sim( 0, 0, ASRTR_TEST_PASS, false, rng );
        asrtr::record     r{};
        CHECK( sim( r ) == ASRTR_SUCCESS );
        CHECK( r.state == ASRTR_TEST_PASS );
}

TEST_CASE( "utest_sim_insta_fail" )
{
        std::mt19937      rng( 0 );
        asrtio::utest_sim sim = make_sim( 0, 0, ASRTR_TEST_FAIL, false, rng );
        asrtr::record     r{};
        CHECK( sim( r ) == ASRTR_SUCCESS );
        CHECK( r.state == ASRTR_TEST_FAIL );
}

TEST_CASE( "utest_sim_running_then_done" )
{
        std::mt19937 rng( 0 );
        // 10 ms fixed duration — first tick will almost certainly be RUNNING
        asrtio::utest_sim sim = make_sim( 10, 0, ASRTR_TEST_PASS, false, rng );
        asrtr::record     r{};
        REQUIRE( sim( r ) == ASRTR_SUCCESS );
        CHECK( r.state == ASRTR_TEST_RUNNING );

        asrtr_test_state final_state = run_to_completion( sim );
        CHECK( final_state == ASRTR_TEST_PASS );
}

TEST_CASE( "utest_sim_variance_clamped_non_negative" )
{
        // duration_ms=0, variance_ms=50 — jitter could go negative; actual_ms must never be < 0
        std::mt19937 rng( 1 );
        for ( int trial = 0; trial < 50; ++trial ) {
                asrtio::utest_sim sim = make_sim( 0, 50, ASRTR_TEST_PASS, false, rng );
                asrtr::record     r{};
                REQUIRE( sim( r ) == ASRTR_SUCCESS );  // triggers actual_ms calculation
                CHECK( sim.actual_ms >= 0 );
        }
}

TEST_CASE( "utest_sim_deterministic" )
{
        // Two sims with same seed must produce identical actual_ms and res
        std::mt19937      rng_a( 42 ), rng_b( 42 );
        asrtio::utest_sim a = make_sim( 300, 200, ASRTR_TEST_PASS, true, rng_a );
        asrtio::utest_sim b = make_sim( 300, 200, ASRTR_TEST_PASS, true, rng_b );

        asrtr::record ra{}, rb{};
        REQUIRE( a( ra ) == ASRTR_SUCCESS );
        REQUIRE( b( rb ) == ASRTR_SUCCESS );

        CHECK( a.actual_ms == b.actual_ms );
        CHECK( a.res == b.res );
}

TEST_CASE( "utest_sim_different_seeds_differ" )
{
        // With randomize=true, different seeds should differ in at least one run
        // over multiple trials.
        bool any_differ = false;
        for ( uint32_t seed = 0; seed < 20; ++seed ) {
                std::mt19937      rng_a( seed ), rng_b( seed + 1000 );
                asrtio::utest_sim a = make_sim( 300, 200, ASRTR_TEST_PASS, true, rng_a );
                asrtio::utest_sim b = make_sim( 300, 200, ASRTR_TEST_PASS, true, rng_b );

                asrtr::record ra{}, rb{};
                REQUIRE( a( ra ) == ASRTR_SUCCESS );
                REQUIRE( b( rb ) == ASRTR_SUCCESS );

                if ( a.actual_ms != b.actual_ms || a.res != b.res ) {
                        any_differ = true;
                        break;
                }
        }
        CHECK( any_differ );
}

TEST_CASE( "utest_sim_name" )
{
        std::mt19937      rng( 0 );
        asrtio::utest_sim sim = make_sim( 0, 0, ASRTR_TEST_PASS, false, rng );
        sim.tname             = "my_test";
        CHECK( std::string_view( sim.name() ) == "my_test" );
}

TEST_CASE( "utest_sim_returns_success" )
{
        std::mt19937      rng( 0 );
        asrtio::utest_sim sim = make_sim( 0, 0, ASRTR_TEST_ERROR, false, rng );
        asrtr::record     r{};
        // operator() must always return ASRTR_SUCCESS regardless of test outcome
        CHECK( sim( r ) == ASRTR_SUCCESS );
}

// ---------------------------------------------------------------------------
// Component 3: cobs_node::on_data

// Build a COBS-framed packet: [u16_le(chid) | payload] COBS-encoded + 0x00 terminator
static std::vector< uint8_t > make_cobs_packet( uint16_t chid, std::vector< uint8_t > payload )
{
        uint8_t  raw[1024];
        uint8_t* pp = raw;
        asrtl_add_u16( &pp, chid );
        for ( auto b : payload )
                *pp++ = b;
        size_t raw_len = pp - raw;

        uint8_t           out[1024];
        struct asrtl_span in_sp{ .b = raw, .e = raw + raw_len };
        struct asrtl_span out_sp{ .b = out, .e = out + sizeof( out ) };
        REQUIRE( asrtl_cobs_encode_buffer( in_sp, &out_sp ) == ASRTL_SUCCESS );

        std::vector< uint8_t > result( out_sp.b, out_sp.e );
        result.push_back( 0x00 );  // COBS frame terminator
        return result;
}

// Minimal cobs_node setup without a real UV socket
static asrtio::cobs_node make_cobs_node( asrtl_node* node, std::function< void( ssize_t ) > on_err )
{
        asrtio::cobs_node cn{};
        cn.node     = node;
        cn.on_error = std::move( on_err );
        asrtl_cobs_ibuffer_init(
            &cn.recv,
            (struct asrtl_span) { .b = cn.ibuffer, .e = cn.ibuffer + sizeof( cn.ibuffer ) } );
        return cn;
}

TEST_CASE( "cobs_on_data_dispatch" )
{
        // Build a node that records what it receives
        struct recv_capture
        {
                uint16_t               chid = 0;
                std::vector< uint8_t > payload;
                int                    call_cnt = 0;
        } cap;

        asrtl_node test_node{};
        test_node.chid     = ASRTL_CORE;
        test_node.recv_ptr = &cap;
        test_node.recv_cb  = []( void* ptr, struct asrtl_span sp )->enum asrtl_status
        {
                auto& c = *static_cast< recv_capture* >( ptr );
                c.payload.assign( sp.b, sp.e );
                ++c.call_cnt;
                return ASRTL_SUCCESS;
        };

        std::vector< uint8_t > payload{ 0x01, 0x02, 0x03 };
        auto                   pkt = make_cobs_packet( ASRTL_CORE, payload );

        bool              err_fired = false;
        asrtio::cobs_node cn        = make_cobs_node( &test_node, [&]( ssize_t ) {
                err_fired = true;
        } );
        cn.on_data( std::span< uint8_t >{ pkt } );

        CHECK( !err_fired );
        CHECK( cap.call_cnt == 1 );
        REQUIRE( cap.payload.size() == payload.size() );
        CHECK( cap.payload == payload );
}

TEST_CASE( "cobs_on_data_error_cb" )
{
        asrtl_node dummy_node{};
        dummy_node.chid    = ASRTL_CORE;
        dummy_node.recv_cb = []( void*, struct asrtl_span )->enum asrtl_status
        {
                return ASRTL_SUCCESS;
        };

        // Random non-COBS bytes with a 0x00 in them to trigger a decode attempt on garbage
        std::vector< uint8_t > junk{ 0x00, 0xFF, 0xAB, 0x00 };

        bool              err_fired = false;
        asrtio::cobs_node cn        = make_cobs_node( &dummy_node, [&]( ssize_t ) {
                err_fired = true;
        } );
        cn.on_data( std::span< uint8_t >{ junk } );

        CHECK( err_fired );
}

TEST_CASE( "cobs_on_data_multi_packet" )
{
        struct recv_capture
        {
                int call_cnt = 0;
        } cap;

        asrtl_node test_node{};
        test_node.chid     = ASRTL_CORE;
        test_node.recv_ptr = &cap;
        test_node.recv_cb  = []( void* ptr, struct asrtl_span )->enum asrtl_status
        {
                ++static_cast< recv_capture* >( ptr )->call_cnt;
                return ASRTL_SUCCESS;
        };

        auto pkt1 = make_cobs_packet( ASRTL_CORE, { 0xAA } );
        auto pkt2 = make_cobs_packet( ASRTL_CORE, { 0xBB, 0xCC } );

        // Concatenate both packets into one buffer
        std::vector< uint8_t > combined;
        combined.insert( combined.end(), pkt1.begin(), pkt1.end() );
        combined.insert( combined.end(), pkt2.begin(), pkt2.end() );

        bool              err_fired = false;
        asrtio::cobs_node cn        = make_cobs_node( &test_node, [&]( ssize_t ) {
                err_fired = true;
        } );
        cn.on_data( std::span< uint8_t >{ combined } );

        CHECK( !err_fired );
        CHECK( cap.call_cnt == 2 );
}

// ---------------------------------------------------------------------------
// Component 4: run_test_suite + cntr_tcp_sys integration

struct recording_reporter : asrtio::suite_reporter
{
        uint32_t                   count = 0;
        std::vector< std::string > starts;
        std::vector< std::string > done_names;
        std::vector< bool >        passed;
        std::vector< double >      durations_ms;
        int                        done_names_at_on_done = -1;

        void on_count( uint32_t n ) override
        {
                count = n;
        }
        void on_test_start( std::string_view n ) override
        {
                starts.emplace_back( n );
        }
        void on_test_done( std::string_view n, bool p, double ms ) override
        {
                done_names.emplace_back( n );
                passed.push_back( p );
                durations_ms.push_back( ms );
        }
        void on_diagnostic( std::string_view, uint32_t ) override
        {
        }
};

// Drain a uv_loop: close any remaining handles, run until empty, then free.
static void drain_loop( uv_loop_t* loop )
{
        uv_walk(
            loop,
            []( uv_handle_t* h, void* ) {
                    if ( !uv_is_closing( h ) )
                            uv_close( h, nullptr );
            },
            nullptr );
        // Run until all close callbacks have fired
        while ( uv_loop_alive( loop ) )
                uv_run( loop, UV_RUN_ONCE );
        // Final run to drain any remaining events
        uv_run( loop, UV_RUN_DEFAULT );
        // uv_loop_delete calls uv_loop_close internally; calling both
        // double-closes and crashes in debug builds (libuv memsets on close).
        uv_loop_delete( loop );
}

struct test_receiver
{
        using receiver_concept = ecor::receiver_t;
        bool*      flag;
        uv_idle_t* idle;

        void set_value() noexcept
        {
                *flag = true;
                uv_idle_stop( idle );
                uv_close( (uv_handle_t*) idle, nullptr );
        }
        void set_error( asrtio::status ) noexcept
        {
                uv_idle_stop( idle );
                uv_close( (uv_handle_t*) idle, nullptr );
        }
        void set_error( ecor::task_error ) noexcept
        {
                uv_idle_stop( idle );
                uv_close( (uv_handle_t*) idle, nullptr );
        }
        void set_stopped() noexcept
        {
                uv_idle_stop( idle );
                uv_close( (uv_handle_t*) idle, nullptr );
        }
        ecor::empty_env get_env() const noexcept
        {
                return {};
        }
};

static asrtio::task< void > suite_coro(
    asrtio::task_ctx&   tctx,
    asrtio::rsim_ctx&   rs,
    recording_reporter& reporter,
    uv_tcp_t&           client )
{
        co_await asrtio::tcp_connect{ &client, "127.0.0.1", rs.port() };
        asrtio::cntr_tcp_sys sys{ &client };
        sys.start();
        co_await asrtio::run_test_suite( tctx, sys, reporter );
        reporter.done_names_at_on_done = (int) reporter.done_names.size();
        sys.close();
        rs.close();
}

struct suite_run
{
        recording_reporter reporter;
        bool               done = false;

        suite_run( uint32_t seed = 42 )
        {
                uv_loop_t*       loop = uv_loop_new();
                asrtio::task_ctx tctx;
                asrtio::rsim_ctx rs{ loop, seed };
                REQUIRE( rs.start() == asrtio::status::success );

                uv_tcp_t client;
                uv_tcp_init( loop, &client );

                uv_idle_t idle;
                idle.data = &tctx;
                uv_idle_init( loop, &idle );
                uv_idle_start( &idle, []( uv_idle_t* h ) {
                        static_cast< asrtio::task_ctx* >( h->data )->tick();
                } );

                auto op = suite_coro( tctx, rs, reporter, client )
                              .connect( test_receiver{ &done, &idle } );
                op.start();

                uv_run( loop, UV_RUN_DEFAULT );
                drain_loop( loop );
        }
};

TEST_CASE( "suite_basic" )
{
        ASRTL_DBG_LOG( "asrtio_test", "Running suite_basic" );
        suite_run r;
        REQUIRE( r.done );
        CHECK( r.reporter.count == 6 );
        CHECK( r.reporter.starts.size() == 6 );
        CHECK( r.reporter.done_names.size() == 6 );
        REQUIRE( r.reporter.done_names_at_on_done != -1 );
        CHECK( r.reporter.done_names_at_on_done == 6 );
        for ( double ms : r.reporter.durations_ms )
                CHECK( ms >= 0.0 );
}

TEST_CASE( "suite_deterministic" )
{
        suite_run a( 42 ), b( 42 );
        REQUIRE( a.reporter.passed.size() == b.reporter.passed.size() );
        CHECK( a.reporter.passed == b.reporter.passed );
}

TEST_CASE( "suite_seed_changes_outcomes" )
{
        suite_run a( 42 ), b( 9999 );
        REQUIRE( a.reporter.passed.size() == b.reporter.passed.size() );
        CHECK( a.reporter.passed != b.reporter.passed );
}

// ---------------------------------------------------------------------------
// Component 5: param protocol over TCP

struct param_received_node
{
        asrtl_flat_id    id;
        std::string      key;
        asrtl_flat_value value;
        asrtl_flat_id    next_sibling;
};

struct param_e2e_state
{
        bool                               param_ready = false;
        asrtl_flat_id                      root_id     = 0;
        std::vector< param_received_node > received;
        int                                errors = 0;

        static void response_cb(
            void*            ptr,
            asrtl_flat_id    id,
            char const*      key,
            asrtl_flat_value value,
            asrtl_flat_id    next_sibling_id )
        {
                auto* s = static_cast< param_e2e_state* >( ptr );
                s->received.push_back( { id, key ? key : "", value, next_sibling_id } );
        }

        static void error_cb( void* ptr, uint8_t, asrtl_flat_id )
        {
                static_cast< param_e2e_state* >( ptr )->errors++;
        }
};

static asrtio::task< void > param_e2e_coro(
    asrtio::task_ctx&      tctx,
    asrtio::rsim_ctx&      rs,
    recording_reporter&    reporter,
    param_e2e_state&       state,
    asrtl_flat_tree const* tree,
    uv_tcp_t&              client )
{
        co_await asrtio::tcp_connect{ &client, "127.0.0.1", rs.port() };
        asrtio::cntr_tcp_sys sys{ &client };
        sys.start();
        sys.set_param_tree( tree, 1u );
        co_await asrtio::run_test_suite( tctx, sys, reporter );

        REQUIRE_FALSE( rs.conns.empty() );
        auto& conn = rs.conns.front();

        while ( !conn.param().ready() )
                co_await ecor::suspend;

        state.param_ready = true;
        state.root_id     = conn.param().root_id();

        std::ignore = conn.param().query(
            state.root_id,
            param_e2e_state::response_cb,
            &state,
            param_e2e_state::error_cb,
            &state );

        while ( state.received.size() < 1 )
                co_await ecor::suspend;

        if ( state.received[0].value.type == ASRTL_FLAT_VALUE_TYPE_OBJECT ) {
                std::ignore = conn.param().query(
                    state.received[0].value.obj_val.first_child,
                    param_e2e_state::response_cb,
                    &state,
                    param_e2e_state::error_cb,
                    &state );
        }

        while ( state.received.size() < 2 )
                co_await ecor::suspend;

        if ( state.received[1].next_sibling != 0 ) {
                std::ignore = conn.param().query(
                    state.received[1].next_sibling,
                    param_e2e_state::response_cb,
                    &state,
                    param_e2e_state::error_cb,
                    &state );
        }

        while ( state.received.size() < 3 )
                co_await ecor::suspend;

        sys.close();
        rs.close();
}

TEST_CASE( "param_tcp_e2e" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 8, 32 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "x", asrtl_flat_value_u32( 42 ) );
        asrtl_flat_tree_append( &tree, 1, 3, "y", asrtl_flat_value_str( "hello" ) );

        param_e2e_state    state;
        recording_reporter reporter;
        bool               done = false;

        uv_loop_t*       loop = uv_loop_new();
        asrtio::task_ctx tctx;
        asrtio::rsim_ctx rs{ loop, 42 };
        REQUIRE( rs.start() == asrtio::status::success );

        uv_tcp_t client;
        uv_tcp_init( loop, &client );

        uv_idle_t idle;
        idle.data = &tctx;
        uv_idle_init( loop, &idle );
        uv_idle_start( &idle, []( uv_idle_t* h ) {
                static_cast< asrtio::task_ctx* >( h->data )->tick();
        } );

        auto op = param_e2e_coro( tctx, rs, reporter, state, &tree, client )
                      .connect( test_receiver{ &done, &idle } );
        op.start();

        uv_run( loop, UV_RUN_DEFAULT );
        drain_loop( loop );

        CHECK( done );
        CHECK( state.param_ready );
        CHECK_EQ( 1u, state.root_id );
        REQUIRE_GE( state.received.size(), 3u );

        // Root node: OBJECT
        CHECK_EQ( 1u, state.received[0].id );
        CHECK_EQ( (uint8_t) ASRTL_FLAT_VALUE_TYPE_OBJECT, (uint8_t) state.received[0].value.type );

        // First child "x": U32 = 42
        CHECK_EQ( "x", state.received[1].key );
        CHECK_EQ( 42u, state.received[1].value.u32_val );

        // Second child "y": STR = "hello"
        CHECK_EQ( "y", state.received[2].key );
        CHECK_EQ( (uint8_t) ASRTL_FLAT_VALUE_TYPE_STR, (uint8_t) state.received[2].value.type );

        CHECK_EQ( 0, state.errors );

        asrtl_flat_tree_deinit( &tree );
}

// ============================================================================
// flat_tree_from_json tests
// ============================================================================

#include <limits>
#include <nlohmann/json.hpp>

static asrtl_flat_query_result query( asrtl_flat_tree& tree, asrtl_flat_id id )
{
        asrtl_flat_query_result r{};
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_query( &tree, id, &r ) );
        return r;
}

TEST_CASE( "ftfj_null" )
{
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK( asrtio::flat_tree_from_json( tree, nlohmann::json( nullptr ) ) );
        auto r = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_NULL, r.value.type );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_bool_true" )
{
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK( asrtio::flat_tree_from_json( tree, nlohmann::json( true ) ) );
        auto r = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_BOOL, r.value.type );
        CHECK_EQ( 1u, r.value.bool_val );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_bool_false" )
{
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK( asrtio::flat_tree_from_json( tree, nlohmann::json( false ) ) );
        auto r = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_BOOL, r.value.type );
        CHECK_EQ( 0u, r.value.bool_val );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_positive_integer" )
{
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK( asrtio::flat_tree_from_json( tree, nlohmann::json( 42 ) ) );
        auto r = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_U32, r.value.type );
        CHECK_EQ( 42u, r.value.u32_val );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_negative_integer" )
{
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK( asrtio::flat_tree_from_json( tree, nlohmann::json( -7 ) ) );
        auto r = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_I32, r.value.type );
        CHECK_EQ( -7, r.value.i32_val );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_unsigned_integer" )
{
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK( asrtio::flat_tree_from_json( tree, nlohmann::json( 0u ) ) );
        auto r = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_U32, r.value.type );
        CHECK_EQ( 0u, r.value.u32_val );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_float" )
{
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK( asrtio::flat_tree_from_json( tree, nlohmann::json( 3.14 ) ) );
        auto r = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_FLOAT, r.value.type );
        CHECK( r.value.float_val == doctest::Approx( 3.14f ) );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_string" )
{
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK( asrtio::flat_tree_from_json( tree, nlohmann::json( "hello" ) ) );
        auto r = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_STR, r.value.type );
        CHECK( strcmp( r.value.str_val, "hello" ) == 0 );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_empty_object" )
{
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK( asrtio::flat_tree_from_json( tree, nlohmann::json::object() ) );
        auto r = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_OBJECT, r.value.type );
        CHECK_EQ( 0u, r.value.obj_val.first_child );
        CHECK_EQ( 0u, r.value.obj_val.last_child );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_empty_array" )
{
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK( asrtio::flat_tree_from_json( tree, nlohmann::json::array() ) );
        auto r = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_ARRAY, r.value.type );
        CHECK_EQ( 0u, r.value.arr_val.first_child );
        CHECK_EQ( 0u, r.value.arr_val.last_child );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_simple_object" )
{
        // {"x": 42, "y": "hi"}
        auto j = nlohmann::json::object( { { "x", 42 }, { "y", "hi" } } );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK( asrtio::flat_tree_from_json( tree, j ) );

        // node 1 = root object
        auto root = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_OBJECT, root.value.type );
        CHECK_EQ( 2u, root.value.obj_val.first_child );
        CHECK_EQ( 3u, root.value.obj_val.last_child );

        // Walk children and verify keys + values
        auto                                             id = root.value.obj_val.first_child;
        std::map< std::string, asrtl_flat_query_result > children;
        while ( id != 0 ) {
                auto r = query( tree, id );
                REQUIRE( r.key != nullptr );
                children[r.key] = r;
                id              = r.next_sibling;
        }
        REQUIRE_EQ( 2u, children.size() );

        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_U32, children["x"].value.type );
        CHECK_EQ( 42u, children["x"].value.u32_val );

        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_STR, children["y"].value.type );
        CHECK( strcmp( children["y"].value.str_val, "hi" ) == 0 );

        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_simple_array" )
{
        // [10, 20, 30]
        auto j = nlohmann::json::array( { 10, 20, 30 } );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK( asrtio::flat_tree_from_json( tree, j ) );

        // node 1 = root array
        auto root = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_ARRAY, root.value.type );

        // Walk children: 3 elements
        asrtl_flat_id id    = root.value.arr_val.first_child;
        int           count = 0;
        uint32_t      vals[3];
        while ( id != 0 ) {
                auto r = query( tree, id );
                CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_U32, r.value.type );
                CHECK( r.key == nullptr );
                vals[count] = r.value.u32_val;
                id          = r.next_sibling;
                count++;
        }
        REQUIRE_EQ( 3, count );
        CHECK_EQ( 10u, vals[0] );
        CHECK_EQ( 20u, vals[1] );
        CHECK_EQ( 30u, vals[2] );

        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_nested_object" )
{
        // {"a": {"b": 1}, "c": [true, false]}
        auto j = nlohmann::json::parse( R"({"a": {"b": 1}, "c": [true, false]})" );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK( asrtio::flat_tree_from_json( tree, j ) );

        // node 1 = root object
        auto root = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_OBJECT, root.value.type );

        // Depth first: ID 1=root, 2="a" obj, 3="b" u32(1), 4="c" arr, 5=true, 6=false
        // "a" is object at ID 2
        auto a = query( tree, 2 );
        CHECK( strcmp( a.key, "a" ) == 0 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_OBJECT, a.value.type );

        // "b" is u32(1) at ID 3
        auto b = query( tree, 3 );
        CHECK( strcmp( b.key, "b" ) == 0 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_U32, b.value.type );
        CHECK_EQ( 1u, b.value.u32_val );

        // "c" is array at ID 4
        auto c = query( tree, 4 );
        CHECK( strcmp( c.key, "c" ) == 0 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_ARRAY, c.value.type );

        // array elements: true at 5, false at 6
        auto t_node = query( tree, 5 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_BOOL, t_node.value.type );
        CHECK_EQ( 1u, t_node.value.bool_val );
        auto f_node = query( tree, 6 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_BOOL, f_node.value.type );
        CHECK_EQ( 0u, f_node.value.bool_val );

        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_mixed_types" )
{
        // {"n": null, "b": true, "i": -3, "u": 100, "f": 1.5, "s": "abc"}
        auto j = nlohmann::json::parse(
            R"({"n": null, "b": true, "i": -3, "u": 100, "f": 1.5, "s": "abc"})" );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK( asrtio::flat_tree_from_json( tree, j ) );

        auto root = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_OBJECT, root.value.type );

        // Walk all children and collect types by key
        asrtl_flat_id id    = root.value.obj_val.first_child;
        int           count = 0;
        while ( id != 0 ) {
                auto r = query( tree, id );
                if ( strcmp( r.key, "n" ) == 0 )
                        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_NULL, r.value.type );
                else if ( strcmp( r.key, "b" ) == 0 ) {
                        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_BOOL, r.value.type );
                        CHECK_EQ( 1u, r.value.bool_val );
                } else if ( strcmp( r.key, "i" ) == 0 ) {
                        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_I32, r.value.type );
                        CHECK_EQ( -3, r.value.i32_val );
                } else if ( strcmp( r.key, "u" ) == 0 ) {
                        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_U32, r.value.type );
                        CHECK_EQ( 100u, r.value.u32_val );
                } else if ( strcmp( r.key, "f" ) == 0 ) {
                        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_FLOAT, r.value.type );
                        CHECK( r.value.float_val == doctest::Approx( 1.5f ) );
                } else if ( strcmp( r.key, "s" ) == 0 ) {
                        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_STR, r.value.type );
                        CHECK( strcmp( r.value.str_val, "abc" ) == 0 );
                }
                id = r.next_sibling;
                count++;
        }
        CHECK_EQ( 6, count );

        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_deeply_nested" )
{
        // {"a": {"b": {"c": 42}}}
        auto j = nlohmann::json::parse( R"({"a": {"b": {"c": 42}}})" );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK( asrtio::flat_tree_from_json( tree, j ) );

        // DFS: 1=root, 2="a" obj, 3="b" obj, 4="c" u32(42)
        auto root = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_OBJECT, root.value.type );

        auto a = query( tree, 2 );
        CHECK( strcmp( a.key, "a" ) == 0 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_OBJECT, a.value.type );

        auto b = query( tree, 3 );
        CHECK( strcmp( b.key, "b" ) == 0 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_OBJECT, b.value.type );

        auto c = query( tree, 4 );
        CHECK( strcmp( c.key, "c" ) == 0 );
        CHECK_EQ( ASRTL_FLAT_VALUE_TYPE_U32, c.value.type );
        CHECK_EQ( 42u, c.value.u32_val );

        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_error_integer_overflow_positive" )
{
        auto j =
            nlohmann::json( static_cast< int64_t >( std::numeric_limits< uint32_t >::max() ) + 1 );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK_FALSE( asrtio::flat_tree_from_json( tree, j ) );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_error_integer_overflow_negative" )
{
        auto j =
            nlohmann::json( static_cast< int64_t >( std::numeric_limits< int32_t >::min() ) - 1 );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK_FALSE( asrtio::flat_tree_from_json( tree, j ) );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_error_unsigned_overflow" )
{
        auto j =
            nlohmann::json( static_cast< uint64_t >( std::numeric_limits< uint32_t >::max() ) + 1 );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK_FALSE( asrtio::flat_tree_from_json( tree, j ) );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_error_float_overflow_positive" )
{
        auto j =
            nlohmann::json( static_cast< double >( std::numeric_limits< float >::max() ) * 2.0 );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK_FALSE( asrtio::flat_tree_from_json( tree, j ) );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_error_float_overflow_negative" )
{
        auto j =
            nlohmann::json( static_cast< double >( -std::numeric_limits< float >::max() ) * 2.0 );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK_FALSE( asrtio::flat_tree_from_json( tree, j ) );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_error_nested_overflow" )
{
        auto j = nlohmann::json::parse(
            R"({"ok": 1, "bad": )" +
            std::to_string(
                static_cast< uint64_t >( std::numeric_limits< uint32_t >::max() ) + 1 ) +
            "}" );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK_FALSE( asrtio::flat_tree_from_json( tree, j ) );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_error_in_array_propagates" )
{
        auto j = nlohmann::json::parse(
            "[1, " +
            std::to_string(
                static_cast< uint64_t >( std::numeric_limits< uint32_t >::max() ) + 1 ) +
            ", 3]" );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        CHECK_FALSE( asrtio::flat_tree_from_json( tree, j ) );
        asrtl_flat_tree_deinit( &tree );
}

#include "../test/stub_allocator.hpp"

TEST_CASE( "ftfj_error_append_alloc_failure" )
{
        stub_allocator_ctx ctx;
        auto               alloc = asrtl_stub_allocator( &ctx );
        asrtl_flat_tree    tree;
        // init succeeds (allocations 1..N), then fail on next alloc during append
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 1, 1 ) );
        // fail on the very next allocation (the block init inside append)
        ctx.fail_at_call = ctx.alloc_calls + 1;

        CHECK_FALSE( asrtio::flat_tree_from_json( tree, nlohmann::json( 42 ) ) );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_error_append_failure_in_nested" )
{
        stub_allocator_ctx ctx;
        auto               alloc = asrtl_stub_allocator( &ctx );
        asrtl_flat_tree    tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 1, 1 ) );
        // Let the root object append succeed, then fail on the child
        ctx.fail_at_call = ctx.alloc_calls + 2;

        auto j = nlohmann::json::parse( R"({"a": 1})" );
        CHECK_FALSE( asrtio::flat_tree_from_json( tree, j ) );
        asrtl_flat_tree_deinit( &tree );
}
