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
#include "../asrtio/output_fs.hpp"
#include "../asrtio/rsim.hpp"
#include "../asrtio/util.hpp"
#include "../asrtl/chann.h"
#include "../asrtl/cobs.h"
#include "../asrtl/flat_tree.h"
#include "../asrtl/log.h"
#include "../asrtl/stream_proto.h"
#include "../asrtl/util.h"
#include "../asrtlpp/util.hpp"
#include "../asrtrpp/reactor.hpp"
#include "stub_fs.hpp"

#include <chrono>
#include <doctest/doctest.h>
#include <map>
#include <span>

using namespace std::chrono_literals;

ASRTL_DEFINE_GPOS_LOG()

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

        uint8_t out[1024];
        struct asrtl_span in_sp
        {
                .b = raw, .e = raw + raw_len
        };
        struct asrtl_span out_sp
        {
                .b = out, .e = out + sizeof( out )
        };
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
            ( struct asrtl_span ){ .b = cn.ibuffer, .e = cn.ibuffer + sizeof( cn.ibuffer ) } );
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
        test_node.e_cb_ptr = &cap;
        test_node.e_cb = []( void* ptr, enum asrtl_event_e event, void* arg ) -> enum asrtl_status {
                if ( event != ASRTL_EVENT_RECV ) return ASRTL_SUCCESS;
                struct asrtl_span sp = *static_cast< struct asrtl_span* >( arg );
                auto& c              = *static_cast< recv_capture* >( ptr );
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
        dummy_node.chid = ASRTL_CORE;
        dummy_node.e_cb = []( void*, enum asrtl_event_e, void* ) -> enum asrtl_status {
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
        test_node.e_cb_ptr = &cap;
        test_node.e_cb     = []( void* ptr, enum asrtl_event_e event, void* ) -> enum asrtl_status {
                if ( event != ASRTL_EVENT_RECV ) return ASRTL_SUCCESS;
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
        std::vector< uint32_t >    start_run_idx;
        std::vector< uint32_t >    start_run_total;
        std::vector< uint32_t >    done_run_idx;
        std::vector< uint32_t >    done_run_total;
        int                        done_names_at_on_done = -1;

        void on_count( uint32_t n ) override { count = n; }
        void on_test_start( std::string_view n, uint32_t ri, uint32_t rt ) override
        {
                starts.emplace_back( n );
                start_run_idx.push_back( ri );
                start_run_total.push_back( rt );
        }
        void on_test_done( std::string_view n, bool p, double ms, uint32_t ri, uint32_t rt )
            override
        {
                done_names.emplace_back( n );
                passed.push_back( p );
                durations_ms.push_back( ms );
                done_run_idx.push_back( ri );
                done_run_total.push_back( rt );
        }
        void on_diagnostic( std::string_view, uint32_t, std::string_view ) override {}
        void on_collect_data( std::string_view, asrtl_flat_tree const* ) override {}
        void on_stream_data( std::string_view, asrt::stream_schemas const& ) override {}
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
                ASRTL_INF_LOG( "asrtio_test", "Test receiver: Task completed successfully" );
        }
        void set_error( asrt::status ) noexcept
        {
                uv_idle_stop( idle );
                uv_close( (uv_handle_t*) idle, nullptr );
                ASRTL_ERR_LOG( "asrtio_test", "Test receiver: Task completed with error" );
        }
        void set_error( ecor::task_error ) noexcept
        {
                uv_idle_stop( idle );
                uv_close( (uv_handle_t*) idle, nullptr );
                ASRTL_ERR_LOG( "asrtio_test", "Test receiver: Task completed with task_error" );
        }
        void set_stopped() noexcept
        {
                uv_idle_stop( idle );
                uv_close( (uv_handle_t*) idle, nullptr );
                ASRTL_INF_LOG( "asrtio_test", "Test receiver: Task stopped" );
        }
        ecor::empty_env get_env() const noexcept { return {}; }
};

static asrtio::task< void > suite_coro(
    asrtio::task_ctx&           tctx,
    uv_loop_t*                  loop,
    uint32_t                    seed,
    asrtio::arena&              arena,
    asrtio::clock&              clk,
    recording_reporter&         reporter,
    std::shared_ptr< uv_tcp_t > client )
{
        auto rs = arena.make< asrtio::rsim_ctx >( loop, seed );
        REQUIRE( rs->start() == ASRTL_SUCCESS );
        co_await asrtio::tcp_connect{ client.get(), "127.0.0.1", rs->port() };
        auto sys = arena.make< asrtio::cntr_tcp_sys >( client, clk );
        sys->start();
        asrtio::param_config no_params;
        asrtio::null_fs      nfs;
        co_await asrtio::run_test_suite( tctx, *sys, reporter, 1000ms, no_params, nfs, {} );
        reporter.done_names_at_on_done = (int) reporter.done_names.size();
}

struct suite_run
{
        recording_reporter reporter;
        bool               done = false;

        suite_run( uint32_t seed = 42 )
        {
                uv_loop_t*                        loop = uv_loop_new();
                asrt::malloc_free_memory_resource mem_res;
                asrtio::task_ctx                  tctx{ mem_res };
                asrtio::arena                     arena{ tctx, mem_res };
                asrtio::steady_clock              clk;

                auto client = std::make_shared< uv_tcp_t >();
                uv_tcp_init( loop, client.get() );

                uv_idle_t idle;
                idle.data = &tctx;
                uv_idle_init( loop, &idle );
                uv_idle_start( &idle, []( uv_idle_t* h ) {
                        static_cast< asrtio::task_ctx* >( h->data )->tick();
                } );

                auto op = ( suite_coro( tctx, loop, seed, arena, clk, reporter, client ) |
                            asrtio::complete_arena( arena ) )
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
        CHECK( r.reporter.count == 23 );
        CHECK( r.reporter.starts.size() == r.reporter.count );
        CHECK( r.reporter.done_names.size() == r.reporter.count );
        REQUIRE( r.reporter.done_names_at_on_done != -1 );
        CHECK( r.reporter.done_names_at_on_done == r.reporter.count );
        for ( double ms : r.reporter.durations_ms ) {
                CHECK( ms >= 0.0 );
                CHECK( ms < 1000.0 );
        }

        // Demo suite: 3 pass (demo_pass, demo_check, demo_counter)
        //             3 fail (demo_fail, demo_check_fail, demo_require_fail)
        //             2 nondeterministic (demo_random, demo_random_counter)
        //             3 param-aware (demo_param_value, demo_param_count, demo_param_find)
        //             9 coroutine-based task demos (incl. collect_demo_task,
        //               stream_demo_task, stream_sensor_demo_task)
        CHECK( r.reporter.done_names[0] == "demo_pass" );
        CHECK( r.reporter.done_names[1] == "demo_fail" );
        CHECK( r.reporter.done_names[2] == "demo_check" );
        CHECK( r.reporter.done_names[3] == "demo_check_fail" );
        CHECK( r.reporter.done_names[4] == "demo_require_fail" );
        CHECK( r.reporter.done_names[5] == "demo_counter" );
        CHECK( r.reporter.done_names[6] == "demo_random" );
        CHECK( r.reporter.done_names[7] == "demo_random_counter" );
        CHECK( r.reporter.done_names[8] == "demo_param_value" );
        CHECK( r.reporter.done_names[9] == "demo_param_count" );
        CHECK( r.reporter.done_names[10] == "demo_param_find" );
        CHECK( r.reporter.done_names[11] == "pass_demo_task" );
        CHECK( r.reporter.done_names[12] == "fail_demo_task" );
        CHECK( r.reporter.done_names[13] == "error_demo_task" );
        CHECK( r.reporter.done_names[14] == "counter_demo_task" );
        CHECK( r.reporter.done_names[15] == "check_demo_task" );
        CHECK( r.reporter.done_names[16] == "check_fail_demo_task" );
        CHECK( r.reporter.done_names[17] == "multi_step_fail_demo_task" );
        CHECK( r.reporter.done_names[18] == "param_query_demo_task" );
        CHECK( r.reporter.done_names[19] == "param_type_overview_task" );
        CHECK( r.reporter.done_names[20] == "collect_demo_task" );
        CHECK( r.reporter.done_names[21] == "stream_demo_task" );
        CHECK( r.reporter.done_names[22] == "stream_sensor_demo_task" );
        CHECK( r.reporter.passed[0] == true );
        CHECK( r.reporter.passed[1] == false );
        CHECK( r.reporter.passed[2] == true );
        CHECK( r.reporter.passed[3] == false );
        CHECK( r.reporter.passed[4] == false );
        CHECK( r.reporter.passed[5] == true );
        CHECK( r.reporter.passed[8] == true );
        CHECK( r.reporter.passed[9] == true );
        CHECK( r.reporter.passed[10] == true );
        CHECK( r.reporter.passed[11] == true );
        CHECK( r.reporter.passed[12] == false );
        CHECK( r.reporter.passed[13] == false );
        CHECK( r.reporter.passed[14] == true );
        CHECK( r.reporter.passed[15] == true );
        CHECK( r.reporter.passed[16] == false );
        CHECK( r.reporter.passed[17] == false );
        CHECK( r.reporter.passed[18] == false );
        CHECK( r.reporter.passed[19] == false );
        CHECK( r.reporter.passed[20] == true );
        CHECK( r.reporter.passed[21] == true );
        CHECK( r.reporter.passed[22] == true );
}

TEST_CASE( "suite_deterministic" )
{
        suite_run a( 42 ), b( 42 );
        REQUIRE( a.reporter.passed.size() == b.reporter.passed.size() );
        CHECK( a.reporter.passed == b.reporter.passed );
}

TEST_CASE( "suite_seed_changes_outcomes" )
{
        // Nondeterministic tests depend on the seed — different seeds should
        // (with high probability) produce different outcomes over multiple trials.
        bool any_differ = false;
        for ( uint32_t s = 0; s < 20 && !any_differ; ++s ) {
                suite_run a( s ), b( s + 1000 );
                REQUIRE( a.reporter.passed.size() == b.reporter.passed.size() );
                if ( a.reporter.passed != b.reporter.passed )
                        any_differ = true;
        }
        CHECK( any_differ );
}

// ---------------------------------------------------------------------------
// Component 5: param protocol over TCP

struct param_received_node
{
        asrt::flat_id    id;
        std::string      key;
        asrtl_flat_value value;
        asrt::flat_id    next_sibling;
};

struct param_e2e_state
{
        bool                               param_ready = false;
        asrt::flat_id                      root_id     = 0;
        std::vector< param_received_node > received;
        int                                errors = 0;
        asrtr_param_query                  query  = {};

        static void query_cb( asrtr_param_client*, asrtr_param_query* q, asrtl_flat_value val )
        {
                auto* s = static_cast< param_e2e_state* >( q->cb_ptr );
                if ( q->error_code != 0 ) {
                        s->errors++;
                } else {
                        s->received.push_back(
                            { q->node_id, q->key ? q->key : "", val, q->next_sibling } );
                }
        }
};

static asrtio::task< void > param_e2e_coro(
    asrtio::task_ctx&           tctx,
    uv_loop_t*                  loop,
    uint32_t                    seed,
    asrtio::arena&              arena,
    asrtio::clock&              clk,
    recording_reporter&         reporter,
    param_e2e_state&            state,
    asrtio::param_config const& params,
    std::shared_ptr< uv_tcp_t > client )
{
        auto rs = arena.make< asrtio::rsim_ctx >( loop, seed );
        REQUIRE( rs->start() == ASRTL_SUCCESS );
        co_await asrtio::tcp_connect{ client.get(), "127.0.0.1", rs->port() };
        auto sys = arena.make< asrtio::cntr_tcp_sys >( client, clk );
        sys->start();
        asrtio::null_fs nfs;
        co_await asrtio::run_test_suite( tctx, *sys, reporter, 1000ms, params, nfs, {} );

        REQUIRE_FALSE( rs->conns.empty() );
        auto& conn = rs->conns.front();

        while ( !asrt::ready( conn.assm.param ) )
                co_await ecor::suspend;

        state.param_ready = true;
        state.root_id     = asrt::root_id( conn.assm.param );

        REQUIRE_EQ(
            ASRTL_SUCCESS,
            asrt::fetch(
                conn.assm.param, &state.query, state.root_id, param_e2e_state::query_cb, &state ) );

        while ( state.received.size() < 1 )
                co_await ecor::suspend;

        if ( state.received[0].value.type == ASRTL_FLAT_CTYPE_OBJECT ) {
                REQUIRE_EQ(
                    ASRTL_SUCCESS,
                    asrt::fetch(
                        conn.assm.param,
                        &state.query,
                        state.received[0].value.data.cont.first_child,
                        param_e2e_state::query_cb,
                        &state ) );
        }

        while ( state.received.size() < 2 )
                co_await ecor::suspend;

        if ( state.received[1].next_sibling != 0 ) {
                REQUIRE_EQ(
                    ASRTL_SUCCESS,
                    asrt::fetch(
                        conn.assm.param,
                        &state.query,
                        state.received[1].next_sibling,
                        param_e2e_state::query_cb,
                        &state ) );
        }

        while ( state.received.size() < 3 )
                co_await ecor::suspend;
}

TEST_CASE( "param_tcp_e2e" )
{
        asrtio::param_config params;
        asrtl_flat_tree_append_cont( &params.tree, 0, 1, nullptr, ASRTL_FLAT_CTYPE_OBJECT );
        asrtl_flat_tree_append_scalar(
            &params.tree, 1, 2, "x", ASRTL_FLAT_STYPE_U32, { .u32_val = 42 } );
        asrtl_flat_tree_append_scalar(
            &params.tree, 1, 3, "y", ASRTL_FLAT_STYPE_STR, { .str_val = "hello" } );
        params.wildcard = { 1 };

        param_e2e_state    state;
        recording_reporter reporter;
        bool               done = false;

        uv_loop_t*                        loop = uv_loop_new();
        asrt::malloc_free_memory_resource mem_res;
        asrtio::task_ctx                  tctx{ mem_res };
        asrtio::arena                     arena{ tctx, mem_res };
        asrtio::steady_clock              clk;

        auto client = std::make_shared< uv_tcp_t >();
        uv_tcp_init( loop, client.get() );

        uv_idle_t idle;
        idle.data = &tctx;
        uv_idle_init( loop, &idle );
        uv_idle_start( &idle, []( uv_idle_t* h ) {
                static_cast< asrtio::task_ctx* >( h->data )->tick();
        } );

        auto op = ( param_e2e_coro( tctx, loop, 42, arena, clk, reporter, state, params, client ) |
                    asrtio::complete_arena( arena ) )
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
        CHECK_EQ( (uint8_t) ASRTL_FLAT_CTYPE_OBJECT, (uint8_t) state.received[0].value.type );

        // First child "x": U32 = 42
        CHECK_EQ( "x", state.received[1].key );
        CHECK_EQ( 42u, state.received[1].value.data.s.u32_val );

        // Second child "y": STR = "hello"
        CHECK_EQ( "y", state.received[2].key );
        CHECK_EQ( (uint8_t) ASRTL_FLAT_STYPE_STR, (uint8_t) state.received[2].value.type );

        CHECK_EQ( 0, state.errors );
}

// ============================================================================
// flat_tree_from_json tests
// ============================================================================

#include <limits>
#include <nlohmann/json.hpp>

static asrtl_flat_query_result query( asrtl_flat_tree& tree, asrt::flat_id id )
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
        asrt::flat_id next_id = 1;
        CHECK( asrtio::flat_tree_from_json( tree, nlohmann::json( nullptr ), next_id ) );
        auto r = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_STYPE_NULL, r.value.type );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_bool_true" )
{
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        CHECK( asrtio::flat_tree_from_json( tree, nlohmann::json( true ), next_id ) );
        auto r = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_STYPE_BOOL, r.value.type );
        CHECK_EQ( 1u, r.value.data.s.bool_val );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_bool_false" )
{
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        CHECK( asrtio::flat_tree_from_json( tree, nlohmann::json( false ), next_id ) );
        auto r = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_STYPE_BOOL, r.value.type );
        CHECK_EQ( 0u, r.value.data.s.bool_val );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_positive_integer" )
{
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        CHECK( asrtio::flat_tree_from_json( tree, nlohmann::json( 42 ), next_id ) );
        auto r = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_STYPE_U32, r.value.type );
        CHECK_EQ( 42u, r.value.data.s.u32_val );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_negative_integer" )
{
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        CHECK( asrtio::flat_tree_from_json( tree, nlohmann::json( -7 ), next_id ) );
        auto r = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_STYPE_I32, r.value.type );
        CHECK_EQ( -7, r.value.data.s.i32_val );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_unsigned_integer" )
{
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        CHECK( asrtio::flat_tree_from_json( tree, nlohmann::json( 0u ), next_id ) );
        auto r = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_STYPE_U32, r.value.type );
        CHECK_EQ( 0u, r.value.data.s.u32_val );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_float" )
{
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        CHECK( asrtio::flat_tree_from_json( tree, nlohmann::json( 3.14 ), next_id ) );
        auto r = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_STYPE_FLOAT, r.value.type );
        CHECK( r.value.data.s.float_val == doctest::Approx( 3.14f ) );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_string" )
{
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        CHECK( asrtio::flat_tree_from_json( tree, nlohmann::json( "hello" ), next_id ) );
        auto r = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_STYPE_STR, r.value.type );
        CHECK( strcmp( r.value.data.s.str_val, "hello" ) == 0 );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_empty_object" )
{
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        CHECK( asrtio::flat_tree_from_json( tree, nlohmann::json::object(), next_id ) );
        auto r = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_CTYPE_OBJECT, r.value.type );
        CHECK_EQ( 0u, r.value.data.cont.first_child );
        CHECK_EQ( 0u, r.value.data.cont.last_child );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_empty_array" )
{
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        CHECK( asrtio::flat_tree_from_json( tree, nlohmann::json::array(), next_id ) );
        auto r = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_CTYPE_ARRAY, r.value.type );
        CHECK_EQ( 0u, r.value.data.cont.first_child );
        CHECK_EQ( 0u, r.value.data.cont.last_child );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_simple_object" )
{
        // {"x": 42, "y": "hi"}
        auto j = nlohmann::json::object( { { "x", 42 }, { "y", "hi" } } );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        CHECK( asrtio::flat_tree_from_json( tree, j, next_id ) );

        // node 1 = root object
        auto root = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_CTYPE_OBJECT, root.value.type );
        CHECK_EQ( 2u, root.value.data.cont.first_child );
        CHECK_EQ( 3u, root.value.data.cont.last_child );

        // Walk children and verify keys + values
        auto                                             id = root.value.data.cont.first_child;
        std::map< std::string, asrtl_flat_query_result > children;
        while ( id != 0 ) {
                auto r = query( tree, id );
                REQUIRE( r.key != nullptr );
                children[r.key] = r;
                id              = r.next_sibling;
        }
        REQUIRE_EQ( 2u, children.size() );

        CHECK_EQ( ASRTL_FLAT_STYPE_U32, children["x"].value.type );
        CHECK_EQ( 42u, children["x"].value.data.s.u32_val );

        CHECK_EQ( ASRTL_FLAT_STYPE_STR, children["y"].value.type );
        CHECK( strcmp( children["y"].value.data.s.str_val, "hi" ) == 0 );

        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_simple_array" )
{
        // [10, 20, 30]
        auto j = nlohmann::json::array( { 10, 20, 30 } );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        CHECK( asrtio::flat_tree_from_json( tree, j, next_id ) );

        // node 1 = root array
        auto root = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_CTYPE_ARRAY, root.value.type );

        // Walk children: 3 elements
        asrt::flat_id id    = root.value.data.cont.first_child;
        int           count = 0;
        uint32_t      vals[3];
        while ( id != 0 ) {
                auto r = query( tree, id );
                CHECK_EQ( ASRTL_FLAT_STYPE_U32, r.value.type );
                CHECK( r.key == nullptr );
                vals[count] = r.value.data.s.u32_val;
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
        asrt::flat_id next_id = 1;
        CHECK( asrtio::flat_tree_from_json( tree, j, next_id ) );

        // node 1 = root object
        auto root = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_CTYPE_OBJECT, root.value.type );

        // Depth first: ID 1=root, 2="a" obj, 3="b" u32(1), 4="c" arr, 5=true, 6=false
        // "a" is object at ID 2
        auto a = query( tree, 2 );
        CHECK( strcmp( a.key, "a" ) == 0 );
        CHECK_EQ( ASRTL_FLAT_CTYPE_OBJECT, a.value.type );

        // "b" is u32(1) at ID 3
        auto b = query( tree, 3 );
        CHECK( strcmp( b.key, "b" ) == 0 );
        CHECK_EQ( ASRTL_FLAT_STYPE_U32, b.value.type );
        CHECK_EQ( 1u, b.value.data.s.u32_val );

        // "c" is array at ID 4
        auto c = query( tree, 4 );
        CHECK( strcmp( c.key, "c" ) == 0 );
        CHECK_EQ( ASRTL_FLAT_CTYPE_ARRAY, c.value.type );

        // array elements: true at 5, false at 6
        auto t_node = query( tree, 5 );
        CHECK_EQ( ASRTL_FLAT_STYPE_BOOL, t_node.value.type );
        CHECK_EQ( 1u, t_node.value.data.s.bool_val );
        auto f_node = query( tree, 6 );
        CHECK_EQ( ASRTL_FLAT_STYPE_BOOL, f_node.value.type );
        CHECK_EQ( 0u, f_node.value.data.s.bool_val );

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
        asrt::flat_id next_id = 1;
        CHECK( asrtio::flat_tree_from_json( tree, j, next_id ) );

        auto root = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_CTYPE_OBJECT, root.value.type );

        // Walk all children and collect types by key
        asrt::flat_id id    = root.value.data.cont.first_child;
        int           count = 0;
        while ( id != 0 ) {
                auto r = query( tree, id );
                if ( strcmp( r.key, "n" ) == 0 )
                        CHECK_EQ( ASRTL_FLAT_STYPE_NULL, r.value.type );
                else if ( strcmp( r.key, "b" ) == 0 ) {
                        CHECK_EQ( ASRTL_FLAT_STYPE_BOOL, r.value.type );
                        CHECK_EQ( 1u, r.value.data.s.bool_val );
                } else if ( strcmp( r.key, "i" ) == 0 ) {
                        CHECK_EQ( ASRTL_FLAT_STYPE_I32, r.value.type );
                        CHECK_EQ( -3, r.value.data.s.i32_val );
                } else if ( strcmp( r.key, "u" ) == 0 ) {
                        CHECK_EQ( ASRTL_FLAT_STYPE_U32, r.value.type );
                        CHECK_EQ( 100u, r.value.data.s.u32_val );
                } else if ( strcmp( r.key, "f" ) == 0 ) {
                        CHECK_EQ( ASRTL_FLAT_STYPE_FLOAT, r.value.type );
                        CHECK( r.value.data.s.float_val == doctest::Approx( 1.5f ) );
                } else if ( strcmp( r.key, "s" ) == 0 ) {
                        CHECK_EQ( ASRTL_FLAT_STYPE_STR, r.value.type );
                        CHECK( strcmp( r.value.data.s.str_val, "abc" ) == 0 );
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
        asrt::flat_id next_id = 1;
        CHECK( asrtio::flat_tree_from_json( tree, j, next_id ) );

        // DFS: 1=root, 2="a" obj, 3="b" obj, 4="c" u32(42)
        auto root = query( tree, 1 );
        CHECK_EQ( ASRTL_FLAT_CTYPE_OBJECT, root.value.type );

        auto a = query( tree, 2 );
        CHECK( strcmp( a.key, "a" ) == 0 );
        CHECK_EQ( ASRTL_FLAT_CTYPE_OBJECT, a.value.type );

        auto b = query( tree, 3 );
        CHECK( strcmp( b.key, "b" ) == 0 );
        CHECK_EQ( ASRTL_FLAT_CTYPE_OBJECT, b.value.type );

        auto c = query( tree, 4 );
        CHECK( strcmp( c.key, "c" ) == 0 );
        CHECK_EQ( ASRTL_FLAT_STYPE_U32, c.value.type );
        CHECK_EQ( 42u, c.value.data.s.u32_val );

        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_error_integer_overflow_positive" )
{
        auto j =
            nlohmann::json( static_cast< int64_t >( std::numeric_limits< uint32_t >::max() ) + 1 );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        CHECK_FALSE( asrtio::flat_tree_from_json( tree, j, next_id ) );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_error_integer_overflow_negative" )
{
        auto j =
            nlohmann::json( static_cast< int64_t >( std::numeric_limits< int32_t >::min() ) - 1 );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        CHECK_FALSE( asrtio::flat_tree_from_json( tree, j, next_id ) );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_error_unsigned_overflow" )
{
        auto j =
            nlohmann::json( static_cast< uint64_t >( std::numeric_limits< uint32_t >::max() ) + 1 );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        CHECK_FALSE( asrtio::flat_tree_from_json( tree, j, next_id ) );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_error_float_overflow_positive" )
{
        auto j =
            nlohmann::json( static_cast< double >( std::numeric_limits< float >::max() ) * 2.0 );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        CHECK_FALSE( asrtio::flat_tree_from_json( tree, j, next_id ) );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "ftfj_error_float_overflow_negative" )
{
        auto j =
            nlohmann::json( static_cast< double >( -std::numeric_limits< float >::max() ) * 2.0 );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        CHECK_FALSE( asrtio::flat_tree_from_json( tree, j, next_id ) );
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
        asrt::flat_id next_id = 1;
        CHECK_FALSE( asrtio::flat_tree_from_json( tree, j, next_id ) );
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
        asrt::flat_id next_id = 1;
        CHECK_FALSE( asrtio::flat_tree_from_json( tree, j, next_id ) );
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

        asrt::flat_id next_id = 1;
        CHECK_FALSE( asrtio::flat_tree_from_json( tree, nlohmann::json( 42 ), next_id ) );
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

        auto          j       = nlohmann::json::parse( R"({"a": 1})" );
        asrt::flat_id next_id = 1;
        CHECK_FALSE( asrtio::flat_tree_from_json( tree, j, next_id ) );
        asrtl_flat_tree_deinit( &tree );
}

// --- flat_tree_to_json tests ---

TEST_CASE( "fttj_null" )
{
        auto            j     = nlohmann::json( nullptr );
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        REQUIRE( asrtio::flat_tree_from_json( tree, j, next_id ) );

        nlohmann::json out;
        CHECK( asrtio::flat_tree_to_json( tree, out ) );
        CHECK( out.is_null() );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "fttj_bool_true" )
{
        auto            j     = nlohmann::json( true );
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        REQUIRE( asrtio::flat_tree_from_json( tree, j, next_id ) );

        nlohmann::json out;
        CHECK( asrtio::flat_tree_to_json( tree, out ) );
        CHECK_EQ( true, out.get< bool >() );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "fttj_bool_false" )
{
        auto            j     = nlohmann::json( false );
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        REQUIRE( asrtio::flat_tree_from_json( tree, j, next_id ) );

        nlohmann::json out;
        CHECK( asrtio::flat_tree_to_json( tree, out ) );
        CHECK_EQ( false, out.get< bool >() );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "fttj_positive_integer" )
{
        auto            j     = nlohmann::json( 42 );
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        REQUIRE( asrtio::flat_tree_from_json( tree, j, next_id ) );

        nlohmann::json out;
        CHECK( asrtio::flat_tree_to_json( tree, out ) );
        CHECK_EQ( 42u, out.get< uint32_t >() );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "fttj_negative_integer" )
{
        auto            j     = nlohmann::json( -7 );
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        REQUIRE( asrtio::flat_tree_from_json( tree, j, next_id ) );

        nlohmann::json out;
        CHECK( asrtio::flat_tree_to_json( tree, out ) );
        CHECK_EQ( -7, out.get< int32_t >() );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "fttj_float" )
{
        auto            j     = nlohmann::json( 3.14f );
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        REQUIRE( asrtio::flat_tree_from_json( tree, j, next_id ) );

        nlohmann::json out;
        CHECK( asrtio::flat_tree_to_json( tree, out ) );
        CHECK_EQ( doctest::Approx( 3.14f ), out.get< float >() );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "fttj_string" )
{
        auto            j     = nlohmann::json( "hello" );
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        REQUIRE( asrtio::flat_tree_from_json( tree, j, next_id ) );

        nlohmann::json out;
        CHECK( asrtio::flat_tree_to_json( tree, out ) );
        CHECK_EQ( "hello", out.get< std::string >() );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "fttj_empty_object" )
{
        auto            j     = nlohmann::json::object();
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        REQUIRE( asrtio::flat_tree_from_json( tree, j, next_id ) );

        nlohmann::json out;
        CHECK( asrtio::flat_tree_to_json( tree, out ) );
        CHECK( out.is_object() );
        CHECK( out.empty() );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "fttj_empty_array" )
{
        auto            j     = nlohmann::json::array();
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        REQUIRE( asrtio::flat_tree_from_json( tree, j, next_id ) );

        nlohmann::json out;
        CHECK( asrtio::flat_tree_to_json( tree, out ) );
        CHECK( out.is_array() );
        CHECK( out.empty() );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "fttj_simple_object" )
{
        auto j = nlohmann::json::object( { { "x", 42 }, { "y", "hi" } } );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        REQUIRE( asrtio::flat_tree_from_json( tree, j, next_id ) );

        nlohmann::json out;
        CHECK( asrtio::flat_tree_to_json( tree, out ) );
        CHECK( out.is_object() );
        CHECK_EQ( 2u, out.size() );
        CHECK_EQ( 42u, out["x"].get< uint32_t >() );
        CHECK_EQ( "hi", out["y"].get< std::string >() );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "fttj_simple_array" )
{
        auto j = nlohmann::json::array( { 10, 20, 30 } );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        REQUIRE( asrtio::flat_tree_from_json( tree, j, next_id ) );

        nlohmann::json out;
        CHECK( asrtio::flat_tree_to_json( tree, out ) );
        CHECK( out.is_array() );
        REQUIRE_EQ( 3u, out.size() );
        CHECK_EQ( 10u, out[0].get< uint32_t >() );
        CHECK_EQ( 20u, out[1].get< uint32_t >() );
        CHECK_EQ( 30u, out[2].get< uint32_t >() );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "fttj_nested_object" )
{
        auto j = nlohmann::json::parse( R"({"a": {"b": 1}, "c": [true, false]})" );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        REQUIRE( asrtio::flat_tree_from_json( tree, j, next_id ) );

        nlohmann::json out;
        CHECK( asrtio::flat_tree_to_json( tree, out ) );
        CHECK_EQ( j, out );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "fttj_mixed_types" )
{
        auto j = nlohmann::json::parse(
            R"({"n": null, "b": true, "i": -5, "u": 100, "f": 1.5, "s": "abc"})" );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        REQUIRE( asrtio::flat_tree_from_json( tree, j, next_id ) );

        nlohmann::json out;
        CHECK( asrtio::flat_tree_to_json( tree, out ) );
        CHECK( out["n"].is_null() );
        CHECK_EQ( true, out["b"].get< bool >() );
        CHECK_EQ( -5, out["i"].get< int32_t >() );
        CHECK_EQ( 100u, out["u"].get< uint32_t >() );
        CHECK_EQ( doctest::Approx( 1.5f ), out["f"].get< float >() );
        CHECK_EQ( "abc", out["s"].get< std::string >() );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "fttj_deeply_nested" )
{
        auto j = nlohmann::json::parse( R"({"a": {"b": {"c": 42}}})" );

        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;
        REQUIRE( asrtio::flat_tree_from_json( tree, j, next_id ) );

        nlohmann::json out;
        CHECK( asrtio::flat_tree_to_json( tree, out ) );
        CHECK_EQ( j, out );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE( "fttj_error_empty_tree" )
{
        asrtl_allocator alloc = asrtl_default_allocator();
        asrtl_flat_tree tree;
        REQUIRE_EQ( ASRTL_SUCCESS, asrtl_flat_tree_init( &tree, alloc, 4, 8 ) );
        asrt::flat_id next_id = 1;

        nlohmann::json out;
        CHECK_FALSE( asrtio::flat_tree_to_json( tree, out ) );
        asrtl_flat_tree_deinit( &tree );
}

// --- param_config runs_for() tests ---

#include "../asrtio/param_config.hpp"

TEST_CASE( "pc_runs_for_listed_test" )
{
        asrtio::param_config cfg;
        cfg.tests["test_a"] = { 1, 3 };

        auto rv = cfg.runs_for( "test_a" );
        CHECK_FALSE( rv.skip );
        REQUIRE_EQ( 2u, rv.roots.size() );
        CHECK_EQ( 1u, rv.roots[0] );
        CHECK_EQ( 3u, rv.roots[1] );
}

TEST_CASE( "pc_runs_for_empty_array_skips" )
{
        asrtio::param_config cfg;
        cfg.tests["test_skip"] = {};

        auto rv = cfg.runs_for( "test_skip" );
        CHECK( rv.skip );
        CHECK( rv.roots.empty() );
}

TEST_CASE( "pc_runs_for_unlisted_with_wildcard" )
{
        asrtio::param_config cfg;
        cfg.wildcard = { 5, 7 };

        auto rv = cfg.runs_for( "unknown_test" );
        CHECK_FALSE( rv.skip );
        REQUIRE_EQ( 2u, rv.roots.size() );
        CHECK_EQ( 5u, rv.roots[0] );
        CHECK_EQ( 7u, rv.roots[1] );
}

TEST_CASE( "pc_runs_for_unlisted_no_wildcard" )
{
        asrtio::param_config cfg;

        auto rv = cfg.runs_for( "unknown_test" );
        CHECK_FALSE( rv.skip );
        REQUIRE_EQ( 1u, rv.roots.size() );
        CHECK_EQ( 0u, rv.roots[0] );
}

TEST_CASE( "pc_runs_for_listed_overrides_wildcard" )
{
        asrtio::param_config cfg;
        cfg.wildcard        = { 5 };
        cfg.tests["test_a"] = { 10 };

        auto rv = cfg.runs_for( "test_a" );
        CHECK_FALSE( rv.skip );
        REQUIRE_EQ( 1u, rv.roots.size() );
        CHECK_EQ( 10u, rv.roots[0] );
}

// --- param_config_from_json() tests ---

#include <nlohmann/json.hpp>

TEST_CASE( "pcfj_multi_test_round_trip" )
{
        auto j = nlohmann::json::parse( R"({
                "test_a": [{"x": 1}, {"x": 2}],
                "test_b": [{"y": true}]
        })" );

        auto opt = asrtio::param_config_from_json( j );
        REQUIRE( opt );
        auto& cfg = *opt;

        // test_a has 2 runs
        REQUIRE_EQ( 2u, cfg.tests.at( "test_a" ).size() );

        // test_b has 1 run
        REQUIRE_EQ( 1u, cfg.tests.at( "test_b" ).size() );

        // round-trip test_a first run
        nlohmann::json out;
        CHECK( asrtio::_flat_tree_to_json_impl( cfg.tree, cfg.tests.at( "test_a" )[0], out ) );
        CHECK_EQ( 1, out["x"].get< int >() );

        // round-trip test_a second run
        nlohmann::json out2;
        CHECK( asrtio::_flat_tree_to_json_impl( cfg.tree, cfg.tests.at( "test_a" )[1], out2 ) );
        CHECK_EQ( 2, out2["x"].get< int >() );

        // round-trip test_b
        nlohmann::json out3;
        CHECK( asrtio::_flat_tree_to_json_impl( cfg.tree, cfg.tests.at( "test_b" )[0], out3 ) );
        CHECK_EQ( true, out3["y"].get< bool >() );
}

TEST_CASE( "pcfj_bare_object_shorthand" )
{
        auto j = nlohmann::json::parse( R"({
                "test_a": {"val": 42}
        })" );

        auto opt = asrtio::param_config_from_json( j );
        REQUIRE( opt );
        auto& cfg = *opt;

        REQUIRE_EQ( 1u, cfg.tests.at( "test_a" ).size() );

        nlohmann::json out;
        CHECK( asrtio::_flat_tree_to_json_impl( cfg.tree, cfg.tests.at( "test_a" )[0], out ) );
        CHECK_EQ( 42, out["val"].get< int >() );
}

TEST_CASE( "pcfj_wildcard" )
{
        auto j = nlohmann::json::parse( R"({
                "*": [{"p": 10}]
        })" );

        auto opt = asrtio::param_config_from_json( j );
        REQUIRE( opt );
        auto& cfg = *opt;

        CHECK( cfg.tests.empty() );
        REQUIRE_EQ( 1u, cfg.wildcard.size() );

        nlohmann::json out;
        CHECK( asrtio::_flat_tree_to_json_impl( cfg.tree, cfg.wildcard[0], out ) );
        CHECK_EQ( 10, out["p"].get< int >() );
}

TEST_CASE( "pcfj_empty_array_skip" )
{
        auto j = nlohmann::json::parse( R"({
                "test_skip": []
        })" );

        auto opt = asrtio::param_config_from_json( j );
        REQUIRE( opt );
        auto& cfg = *opt;

        REQUIRE( cfg.tests.count( "test_skip" ) );
        CHECK( cfg.tests.at( "test_skip" ).empty() );
}

TEST_CASE( "pcfj_error_non_object_top" )
{
        auto j = nlohmann::json::parse( R"([1, 2, 3])" );
        CHECK_FALSE( asrtio::param_config_from_json( j ) );
}

TEST_CASE( "pcfj_error_string_value" )
{
        auto j = nlohmann::json::parse( R"({"test_a": "bad"})" );
        CHECK_FALSE( asrtio::param_config_from_json( j ) );
}

TEST_CASE( "pcfj_error_non_object_array_elem" )
{
        auto j = nlohmann::json::parse( R"({"test_a": [42]})" );
        CHECK_FALSE( asrtio::param_config_from_json( j ) );
}

// --- param_config_from_stream() tests ---

#include <sstream>

TEST_CASE( "pcfs_load_valid" )
{
        std::istringstream in( R"({"test_a": [{"x": 1}]})" );
        auto               opt = asrtio::param_config_from_stream( in );

        REQUIRE( opt );
        auto rv = opt->runs_for( "test_a" );
        CHECK_FALSE( rv.skip );
        REQUIRE_EQ( 1u, rv.roots.size() );
}

TEST_CASE( "pcfs_invalid_json" )
{
        std::istringstream in( "{ not valid json }}}" );
        CHECK_FALSE( asrtio::param_config_from_stream( in ) );
}

TEST_CASE( "pcff_nonexistent_file" )
{
        CHECK_FALSE( asrtio::param_config_from_file( "no_such_file_12345.json" ) );
}

// ---------------------------------------------------------------------------
// Component 8: rsim integration tests for param_config

static asrtio::task< void > suite_param_coro(
    asrtio::task_ctx&           tctx,
    uv_loop_t*                  loop,
    uint32_t                    seed,
    asrtio::arena&              arena,
    asrtio::clock&              clk,
    recording_reporter&         reporter,
    asrtio::param_config&       params,
    std::shared_ptr< uv_tcp_t > client )
{
        auto rs = arena.make< asrtio::rsim_ctx >( loop, seed );
        REQUIRE( rs->start() == ASRTL_SUCCESS );
        co_await asrtio::tcp_connect{ client.get(), "127.0.0.1", rs->port() };
        auto sys = arena.make< asrtio::cntr_tcp_sys >( client, clk );
        sys->start();
        asrtio::null_fs nfs;
        co_await asrtio::run_test_suite( tctx, *sys, reporter, 1000ms, params, nfs, {} );
}

struct param_suite_run
{
        recording_reporter reporter;
        bool               done = false;

        param_suite_run( asrtio::param_config& params, uint32_t seed = 42 )
        {
                uv_loop_t*                        loop = uv_loop_new();
                asrt::malloc_free_memory_resource mem_res;
                asrtio::task_ctx                  tctx{ mem_res };
                asrtio::arena                     arena{ tctx, mem_res };
                asrtio::steady_clock              clk;

                auto client = std::make_shared< uv_tcp_t >();
                uv_tcp_init( loop, client.get() );

                uv_idle_t idle;
                idle.data = &tctx;
                uv_idle_init( loop, &idle );
                uv_idle_start( &idle, []( uv_idle_t* h ) {
                        static_cast< asrtio::task_ctx* >( h->data )->tick();
                } );

                auto op =
                    ( suite_param_coro( tctx, loop, 42, arena, clk, reporter, params, client ) |
                      asrtio::complete_arena( arena ) )
                        .connect( test_receiver{ &done, &idle } );
                op.start();

                uv_run( loop, UV_RUN_DEFAULT );
                drain_loop( loop );
        }
};

TEST_CASE( "suite_param_multi_run" )
{
        // demo_pass mapped to 2 param sets → runs twice
        auto cfg_opt = asrtio::param_config_from_json( nlohmann::json::parse( R"(
        {
                "demo_pass": [{"a": 1}, {"b": 2}]
        }
        )" ) );
        REQUIRE( cfg_opt );

        param_suite_run r( *cfg_opt );
        REQUIRE( r.done );

        CHECK_EQ( r.reporter.count + 1, r.reporter.done_names.size() );

        // Find the two demo_pass entries
        std::vector< size_t > ip_indices;
        for ( size_t i = 0; i < r.reporter.done_names.size(); ++i )
                if ( r.reporter.done_names[i] == "demo_pass" )
                        ip_indices.push_back( i );
        REQUIRE_EQ( 2u, ip_indices.size() );
        // They should be consecutive
        CHECK_EQ( ip_indices[0] + 1, ip_indices[1] );
        // run_idx should be 1 and 2, run_total should be 2
        CHECK_EQ( 1u, r.reporter.done_run_idx[ip_indices[0]] );
        CHECK_EQ( 2u, r.reporter.done_run_total[ip_indices[0]] );
        CHECK_EQ( 2u, r.reporter.done_run_idx[ip_indices[1]] );
        CHECK_EQ( 2u, r.reporter.done_run_total[ip_indices[1]] );
}

TEST_CASE( "suite_param_skip" )
{
        // demo_pass mapped to [] → skipped entirely
        auto cfg_opt = asrtio::param_config_from_json( nlohmann::json::parse( R"(
        {
                "demo_pass": []
        }
        )" ) );
        REQUIRE( cfg_opt );

        param_suite_run r( *cfg_opt );
        REQUIRE( r.done );

        CHECK_EQ( r.reporter.count - 1, r.reporter.done_names.size() );
        // demo_pass should not appear
        for ( auto const& name : r.reporter.done_names )
                CHECK_NE( name, "demo_pass" );
        for ( auto const& name : r.reporter.starts )
                CHECK_NE( name, "demo_pass" );
}

TEST_CASE( "suite_param_wildcard" )
{
        // "*" applies to all unlisted tests; "demo_pass" has its own entry
        auto cfg_opt = asrtio::param_config_from_json( nlohmann::json::parse( R"(
        {
                "*": {"w": 99},
                "demo_pass": [{"a": 1}, {"b": 2}]
        }
        )" ) );
        REQUIRE( cfg_opt );

        param_suite_run r( *cfg_opt );
        REQUIRE( r.done );

        // 18 unlisted tests get 1 run each (via wildcard), demo_pass gets 2 → 20
        CHECK_EQ( r.reporter.count + 1, r.reporter.done_names.size() );

        // demo_pass appears exactly twice
        uint32_t ip_count = 0;
        for ( auto const& name : r.reporter.done_names )
                if ( name == "demo_pass" )
                        ++ip_count;
        CHECK_EQ( 2u, ip_count );

        // Each wildcard test gets [1/1]
        for ( size_t i = 0; i < r.reporter.done_names.size(); ++i ) {
                if ( r.reporter.done_names[i] != "demo_pass" ) {
                        CHECK_EQ( 1u, r.reporter.done_run_idx[i] );
                        CHECK_EQ( 1u, r.reporter.done_run_total[i] );
                }
        }
}

TEST_CASE( "suite_param_unknown_key" )
{
        // "nonexistent_test" is not a device test — should log a warning but not fail
        auto cfg_opt = asrtio::param_config_from_json( nlohmann::json::parse( R"(
        {
                "nonexistent_test": [{"a": 1}]
        }
        )" ) );
        REQUIRE( cfg_opt );

        param_suite_run r( *cfg_opt );
        REQUIRE( r.done );

        CHECK_EQ( r.reporter.count, r.reporter.done_names.size() );
        // "nonexistent_test" never appears in results
        for ( auto const& name : r.reporter.done_names )
                CHECK_NE( name, "nonexistent_test" );
}

// ---------------------------------------------------------------------------
// Component 9: run_test_suite output file verification
// ---------------------------------------------------------------------------

template < typename CoroFactory >
static bool run_coro( CoroFactory&& factory )
{
        uv_loop_t*                        loop = uv_loop_new();
        asrt::malloc_free_memory_resource mem_res;
        asrtio::task_ctx                  tctx{ mem_res };
        asrtio::arena                     arena{ tctx, mem_res };
        asrtio::steady_clock              clk;

        auto client = std::make_shared< uv_tcp_t >();
        uv_tcp_init( loop, client.get() );

        uv_idle_t idle;
        idle.data = &tctx;
        uv_idle_init( loop, &idle );
        uv_idle_start( &idle, []( uv_idle_t* h ) {
                static_cast< asrtio::task_ctx* >( h->data )->tick();
        } );

        bool done = false;
        auto op   = ( factory( tctx, loop, arena, clk, client ) | asrtio::complete_arena( arena ) )
                      .connect( test_receiver{ &done, &idle } );
        op.start();

        uv_run( loop, UV_RUN_DEFAULT );
        drain_loop( loop );
        return done;
}

static asrtio::task< void > suite_output_coro(
    asrtio::task_ctx&           tctx,
    uv_loop_t*                  loop,
    asrtio::arena&              arena,
    asrtio::clock&              clk,
    recording_reporter&         reporter,
    stub_fs&                    sfs,
    std::shared_ptr< uv_tcp_t > client )
{
        auto rs = arena.make< asrtio::rsim_ctx >( loop, 42u );
        REQUIRE( rs->start() == ASRTL_SUCCESS );
        co_await asrtio::tcp_connect{ client.get(), "127.0.0.1", rs->port() };
        auto sys = arena.make< asrtio::cntr_tcp_sys >( client, clk );
        sys->start();
        asrtio::param_config no_params;
        co_await asrtio::run_test_suite( tctx, *sys, reporter, 1000ms, no_params, sfs, "out" );
}

TEST_CASE( "suite_output_files" )
{
        recording_reporter reporter;
        stub_fs            sfs;

        bool done = run_coro( [&]( asrtio::task_ctx&           tctx,
                                   uv_loop_t*                  loop,
                                   asrtio::arena&              arena,
                                   asrtio::clock&              clk,
                                   std::shared_ptr< uv_tcp_t > client ) {
                return suite_output_coro( tctx, loop, arena, clk, reporter, sfs, client );
        } );

        REQUIRE( done );

        // Directories were created for test runs
        CHECK_FALSE( sfs.dirs.empty() );

        // --- diag.csv for demo_fail: contains the intentional failure record ---
        {
                auto it = sfs.files.find( "out/demo_fail/0/diag.csv" );
                REQUIRE( it != sfs.files.end() );
                auto const& csv = it->second;
                CHECK( csv.find( "file,line,extra" ) != std::string::npos );
                CHECK( csv.find( "demo.hpp" ) != std::string::npos );
                CHECK( csv.find( "intentional" ) != std::string::npos );
        }

        // --- diag.csv for demo_pass: header only, no failure records ---
        {
                auto it = sfs.files.find( "out/demo_pass/0/diag.csv" );
                REQUIRE( it != sfs.files.end() );
                CHECK_EQ( std::string{ "file,line,extra\n" }, it->second );
        }

        // --- collect.json for collect_demo_task ---
        {
                auto it = sfs.files.find( "out/collect_demo_task/0/collect.json" );
                REQUIRE( it != sfs.files.end() );
                auto const& content = it->second;
                auto        j       = nlohmann::json::parse( content );
                CHECK( j.is_object() );
                CHECK( content.find( "\"value\"" ) != std::string::npos );
                CHECK( content.find( "42" ) != std::string::npos );
                CHECK( content.find( "\"tag\"" ) != std::string::npos );
                CHECK( content.find( "\"demo\"" ) != std::string::npos );
        }

        // --- stream.0.csv for stream_demo_task ---
        {
                auto it = sfs.files.find( "out/stream_demo_task/0/stream.0.csv" );
                REQUIRE( it != sfs.files.end() );
                auto const& csv = it->second;
                CHECK( csv.find( "u32,float" ) != std::string::npos );
                // 3 data rows: (0,0), (100,1.5), (200,3)
                CHECK( csv.find( "\n0,0\n" ) != std::string::npos );
                CHECK( csv.find( "\n100,1.5\n" ) != std::string::npos );
                CHECK( csv.find( "\n200,3\n" ) != std::string::npos );
        }

        // --- stream.0.csv for stream_sensor_demo_task ---
        {
                auto it = sfs.files.find( "out/stream_sensor_demo_task/0/stream.0.csv" );
                REQUIRE( it != sfs.files.end() );
                auto const& csv = it->second;
                CHECK( csv.find( "u32,float,u8,u8" ) != std::string::npos );
                // 100 data rows; spot-check first and last
                CHECK( csv.find( "\n0,20," ) != std::string::npos );
                CHECK( csv.find( "\n990,29.9" ) != std::string::npos );
        }

        // --- no collect.json for tests that don't collect ---
        CHECK( sfs.files.find( "out/demo_pass/0/collect.json" ) == sfs.files.end() );

        // --- no stream CSVs for tests that don't stream ---
        bool has_stream_for_pass = false;
        for ( auto const& [path, _] : sfs.files )
                if ( path.find( "demo_pass/0/stream." ) != std::string::npos )
                        has_stream_for_pass = true;
        CHECK_FALSE( has_stream_for_pass );
}

// ---------------------------------------------------------------------------
// write_stream_csv tests
// ---------------------------------------------------------------------------

TEST_CASE( "write_stream_csv: single u8 field" )
{
        stub_fs fs;

        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_U8 };
        uint8_t                      data[]   = { 42 };
        asrtc_stream_record          rec      = { .next = nullptr, .data = data };
        asrtc_stream_schema          sc       = {
                           .schema_id   = 0,
                           .field_count = 1,
                           .record_size = 1,
                           .fields      = fields,
                           .first       = &rec,
                           .last        = &rec,
                           .count       = 1,
        };

        asrtio::write_stream_csv( fs, "out/stream.0.csv", sc );

        REQUIRE( fs.files.count( "out/stream.0.csv" ) == 1 );
        CHECK_EQ( fs.files["out/stream.0.csv"], "u8\n42\n" );
}

TEST_CASE( "write_stream_csv: multi-field u32,i8" )
{
        stub_fs fs;

        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_U32, ASRTL_STRM_FIELD_I8 };
        uint8_t                      data[5];
        uint8_t*                     p = data;
        asrtl_add_u32( &p, 1000 );
        *p++ = static_cast< uint8_t >( static_cast< int8_t >( -3 ) );

        asrtc_stream_record rec = { .next = nullptr, .data = data };
        asrtc_stream_schema sc  = {
             .schema_id   = 5,
             .field_count = 2,
             .record_size = 5,
             .fields      = fields,
             .first       = &rec,
             .last        = &rec,
             .count       = 1,
        };

        asrtio::write_stream_csv( fs, "out/stream.5.csv", sc );

        REQUIRE( fs.files.count( "out/stream.5.csv" ) == 1 );
        CHECK_EQ( fs.files["out/stream.5.csv"], "u32,i8\n1000,-3\n" );
}

TEST_CASE( "write_stream_csv: multiple records" )
{
        stub_fs fs;

        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_U16 };
        uint8_t                      data1[2];
        uint8_t                      data2[2];
        uint8_t*                     p1 = data1;
        uint8_t*                     p2 = data2;
        asrtl_add_u16( &p1, 100 );
        asrtl_add_u16( &p2, 200 );

        asrtc_stream_record rec2 = { .next = nullptr, .data = data2 };
        asrtc_stream_record rec1 = { .next = &rec2, .data = data1 };
        asrtc_stream_schema sc   = {
              .schema_id   = 0,
              .field_count = 1,
              .record_size = 2,
              .fields      = fields,
              .first       = &rec1,
              .last        = &rec2,
              .count       = 2,
        };

        asrtio::write_stream_csv( fs, "s.csv", sc );

        REQUIRE( fs.files.count( "s.csv" ) == 1 );
        CHECK_EQ( fs.files["s.csv"], "u16\n100\n200\n" );
}

TEST_CASE( "write_stream_csv: empty schema (no records)" )
{
        stub_fs fs;

        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_BOOL };
        asrtc_stream_schema          sc       = {
                           .schema_id   = 0,
                           .field_count = 1,
                           .record_size = 1,
                           .fields      = fields,
                           .first       = nullptr,
                           .last        = nullptr,
                           .count       = 0,
        };

        asrtio::write_stream_csv( fs, "empty.csv", sc );

        REQUIRE( fs.files.count( "empty.csv" ) == 1 );
        CHECK_EQ( fs.files["empty.csv"], "bool\n" );
}

TEST_CASE( "write_stream_csv: float field" )
{
        stub_fs fs;

        enum asrtl_strm_field_type_e fields[] = { ASRTL_STRM_FIELD_FLOAT };
        uint8_t                      data[4];
        uint8_t*                     p   = data;
        float                        val = 3.14f;
        uint32_t                     bits;
        std::memcpy( &bits, &val, 4 );
        asrtl_add_u32( &p, bits );

        asrtc_stream_record rec = { .next = nullptr, .data = data };
        asrtc_stream_schema sc  = {
             .schema_id   = 0,
             .field_count = 1,
             .record_size = 4,
             .fields      = fields,
             .first       = &rec,
             .last        = &rec,
             .count       = 1,
        };

        asrtio::write_stream_csv( fs, "f.csv", sc );

        REQUIRE( fs.files.count( "f.csv" ) == 1 );
        // Check that it starts with the header and a reasonable float value
        auto const& content = fs.files["f.csv"];
        CHECK( content.starts_with( "float\n" ) );
        CHECK( content.find( "3.14" ) != std::string::npos );
}

// ---------------------------------------------------------------------------
// write_strm_field tests
// ---------------------------------------------------------------------------

TEST_CASE( "write_strm_field: all types" )
{
        SUBCASE( "u8" )
        {
                uint8_t            data[] = { 255 };
                uint8_t*           p      = data;
                std::ostringstream os;
                asrtio::write_strm_field( os, ASRTL_STRM_FIELD_U8, p );
                CHECK_EQ( os.str(), "255" );
                CHECK_EQ( p, data + 1 );
        }
        SUBCASE( "u16" )
        {
                uint8_t  data[2];
                uint8_t* p = data;
                asrtl_add_u16( &p, 1234 );
                p = data;
                std::ostringstream os;
                asrtio::write_strm_field( os, ASRTL_STRM_FIELD_U16, p );
                CHECK_EQ( os.str(), "1234" );
                CHECK_EQ( p, data + 2 );
        }
        SUBCASE( "u32" )
        {
                uint8_t  data[4];
                uint8_t* p = data;
                asrtl_add_u32( &p, 70000 );
                p = data;
                std::ostringstream os;
                asrtio::write_strm_field( os, ASRTL_STRM_FIELD_U32, p );
                CHECK_EQ( os.str(), "70000" );
                CHECK_EQ( p, data + 4 );
        }
        SUBCASE( "i8" )
        {
                uint8_t  data[] = { static_cast< uint8_t >( static_cast< int8_t >( -1 ) ) };
                uint8_t* p      = data;
                std::ostringstream os;
                asrtio::write_strm_field( os, ASRTL_STRM_FIELD_I8, p );
                CHECK_EQ( os.str(), "-1" );
                CHECK_EQ( p, data + 1 );
        }
        SUBCASE( "i16" )
        {
                uint8_t  data[2];
                uint8_t* p = data;
                asrtl_add_u16( &p, static_cast< uint16_t >( static_cast< int16_t >( -500 ) ) );
                p = data;
                std::ostringstream os;
                asrtio::write_strm_field( os, ASRTL_STRM_FIELD_I16, p );
                CHECK_EQ( os.str(), "-500" );
                CHECK_EQ( p, data + 2 );
        }
        SUBCASE( "i32" )
        {
                uint8_t  data[4];
                uint8_t* p = data;
                asrtl_add_i32( &p, -100000 );
                p = data;
                std::ostringstream os;
                asrtio::write_strm_field( os, ASRTL_STRM_FIELD_I32, p );
                CHECK_EQ( os.str(), "-100000" );
                CHECK_EQ( p, data + 4 );
        }
        SUBCASE( "bool true" )
        {
                uint8_t            data[] = { 1 };
                uint8_t*           p      = data;
                std::ostringstream os;
                asrtio::write_strm_field( os, ASRTL_STRM_FIELD_BOOL, p );
                CHECK_EQ( os.str(), "true" );
        }
        SUBCASE( "bool false" )
        {
                uint8_t            data[] = { 0 };
                uint8_t*           p      = data;
                std::ostringstream os;
                asrtio::write_strm_field( os, ASRTL_STRM_FIELD_BOOL, p );
                CHECK_EQ( os.str(), "false" );
        }
}

// ---------------------------------------------------------------------------
// asrtl_strm_field_type_to_str tests
// ---------------------------------------------------------------------------

TEST_CASE( "strm_field_type_to_str" )
{
        CHECK_EQ( std::string( asrtl_strm_field_type_to_str( ASRTL_STRM_FIELD_U8 ) ), "u8" );
        CHECK_EQ( std::string( asrtl_strm_field_type_to_str( ASRTL_STRM_FIELD_U16 ) ), "u16" );
        CHECK_EQ( std::string( asrtl_strm_field_type_to_str( ASRTL_STRM_FIELD_U32 ) ), "u32" );
        CHECK_EQ( std::string( asrtl_strm_field_type_to_str( ASRTL_STRM_FIELD_I8 ) ), "i8" );
        CHECK_EQ( std::string( asrtl_strm_field_type_to_str( ASRTL_STRM_FIELD_I16 ) ), "i16" );
        CHECK_EQ( std::string( asrtl_strm_field_type_to_str( ASRTL_STRM_FIELD_I32 ) ), "i32" );
        CHECK_EQ( std::string( asrtl_strm_field_type_to_str( ASRTL_STRM_FIELD_FLOAT ) ), "float" );
        CHECK_EQ( std::string( asrtl_strm_field_type_to_str( ASRTL_STRM_FIELD_BOOL ) ), "bool" );
        CHECK_EQ( std::string( asrtl_strm_field_type_to_str( ASRTL_STRM_FIELD_LBRACKET ) ), "[" );
        CHECK_EQ( std::string( asrtl_strm_field_type_to_str( ASRTL_STRM_FIELD_RBRACKET ) ), "]" );
        CHECK_EQ(
            std::string( asrtl_strm_field_type_to_str( (enum asrtl_strm_field_type_e) 0xFF ) ),
            "?" );
}
