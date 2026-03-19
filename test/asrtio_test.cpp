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
#include "../asrtl/log.h"
#include "../asrtl/util.h"
#include "../asrtlpp/util.hpp"
#include "../asrtrpp/reactor.hpp"

#include <chrono>
#include <doctest/doctest.h>
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
// Component 2 infrastructure: in-process controller <-> reactor fixture

static std::vector< uint8_t > flatten_span( asrtl::rec_span const* buff )
{
        std::vector< uint8_t > v;
        for ( auto const* seg = buff; seg; seg = seg->next )
                v.insert( v.end(), seg->b, seg->e );
        return v;
}

struct pass_cb
{
        char const* name() const
        {
                return "pass_cb";
        }
        asrtr::status operator()( asrtr::record& r )
        {
                r.state = ASRTR_TEST_PASS;
                return ASRTR_SUCCESS;
        }
};

struct fail_cb
{
        char const* name() const
        {
                return "fail_cb";
        }
        asrtr::status operator()( asrtr::record& r )
        {
                r.state = ASRTR_TEST_FAIL;
                return ASRTR_SUCCESS;
        }
};

struct task_ctx
{
        asrtl::opt< asrtc::controller > c;

        asrtr::unit< pass_cb > t0;
        asrtr::unit< fail_cb > t1;

        std::function< asrtl_status( asrtl_chann_id, asrtl_rec_span* ) > r_send;
        std::function< asrtl_status( asrtl_chann_id, asrtl_rec_span* ) > c_send =
            [this]( asrtl_chann_id, asrtl_rec_span* buff ) {
                    auto  flat = flatten_span( buff );
                    auto  sp   = asrtl::cnv( std::span{ flat } );
                    auto* rn   = r.node();
                    rn->recv_cb( rn->recv_ptr, sp );
                    return ASRTL_SUCCESS;
            };
        asrtr::reactor r;

        task_ctx()
          : r_send( [this]( asrtl_chann_id, asrtl_rec_span* buff ) {
                  auto  flat = flatten_span( buff );
                  auto  sp   = asrtl::cnv( std::span{ flat } );
                  auto* cn   = ( *c ).node();
                  cn->recv_cb( cn->recv_ptr, sp );
                  return ASRTL_SUCCESS;
          } )
          , r( r_send, "task_reactor" )
        {
                r.add_test( t0 );
                r.add_test( t1 );

                c.emplace(
                    c_send,
                    []( asrtl::source, asrtl::ecode ) -> asrtc::status {
                            return ASRTC_SUCCESS;
                    },
                    []( asrtc::status ) -> asrtc::status {
                            return ASRTC_SUCCESS;
                    } );

                spin();
        }

        void spin( int limit = 200 )
        {
                for ( int i = 0; i < limit && !c->is_idle(); ++i ) {
                        (void) c->tick();
                        r.tick();
                }
                REQUIRE( c->is_idle() );
        }

        void spin_task( asrtio::task& t, int limit = 500 )
        {
                for ( int i = 0; i < limit; ++i ) {
                        if ( t.tick() == asrtio::task::finished )
                                return;
                        (void) c->tick();
                        r.tick();
                }
        }
};

// ---------------------------------------------------------------------------
// call_function_task

TEST_CASE( "call_task_fires_once" )
{
        int                        count = 0;
        asrtio::call_function_task t( [&] {
                ++count;
        } );
        CHECK( t.tick() == asrtio::task::finished );
        CHECK( count == 1 );
}

TEST_CASE( "call_task_second_tick_safe" )
{
        int                        count = 0;
        asrtio::call_function_task t( [&] {
                ++count;
        } );
        t.tick();
        CHECK( t.tick() == asrtio::task::finished );
        CHECK( count == 1 );  // still 1, func was cleared
}

// ---------------------------------------------------------------------------
// run_test_task

TEST_CASE( "run_task_waits_while_busy" )
{
        task_ctx ctx;
        // Kick off a query to make controller busy
        (void) ctx.c->query_test_count( []( asrtc::status, uint32_t ) {
                return ASRTC_SUCCESS;
        } );
        REQUIRE( !ctx.c->is_idle() );

        asrtio::run_test_task t( *ctx.c, 0 );
        CHECK( t.tick() == asrtio::task::runnning );

        ctx.spin();
}

TEST_CASE( "run_task_on_start_fires_once" )
{
        task_ctx ctx;

        int                   start_count = 0;
        asrtio::run_test_task t(
            *ctx.c,
            0,
            [&] {
                    ++start_count;
            },
            {} );

        // tick until finished
        ctx.spin_task( t );
        CHECK( start_count == 1 );
}

TEST_CASE( "run_task_result_pass" )
{
        task_ctx              ctx;
        asrtc::result         got{};
        bool                  got_result = false;
        asrtio::run_test_task t( *ctx.c, 0, {}, [&]( asrtc::result const& res ) {
                got        = res;
                got_result = true;
        } );

        ctx.spin_task( t );
        REQUIRE( got_result );
        CHECK( got.res == ASRTC_TEST_SUCCESS );
}

TEST_CASE( "run_task_result_fail" )
{
        task_ctx              ctx;
        asrtc::result         got{};
        bool                  got_result = false;
        asrtio::run_test_task t(
            *ctx.c,
            1,  // t1 is fail_cb
            {},
            [&]( asrtc::result const& res ) {
                    got        = res;
                    got_result = true;
            } );

        ctx.spin_task( t );
        REQUIRE( got_result );
        CHECK( got.res == ASRTC_TEST_FAILURE );
}

TEST_CASE( "run_task_finished_after_result" )
{
        task_ctx              ctx;
        asrtio::run_test_task t( *ctx.c, 0 );
        asrtio::task::res     last = asrtio::task::runnning;
        ctx.spin_task( t );
        last = t.tick();  // one more tick — must stay finished
        CHECK( last == asrtio::task::finished );
}

// ---------------------------------------------------------------------------
// test_pool_task

TEST_CASE( "pool_task_count_fires" )
{
        task_ctx ctx;
        uint32_t received_count = 0;

        asrtio::test_pool_task t( *ctx.c, []( uint32_t, std::string_view ) {} );
        t.on_count = [&]( uint32_t c ) {
                received_count = c;
        };

        ctx.spin_task( t );
        CHECK( received_count == 2 );  // t0 + t1
}

TEST_CASE( "pool_task_cb_each_test" )
{
        task_ctx                                          ctx;
        std::vector< std::pair< uint32_t, std::string > > seen;

        asrtio::test_pool_task t( *ctx.c, [&]( uint32_t id, std::string_view name ) {
                seen.emplace_back( id, name );
        } );

        ctx.spin_task( t );
        REQUIRE( seen.size() == 2 );
        CHECK( seen[0].second == "pass_cb" );
        CHECK( seen[1].second == "fail_cb" );
}

TEST_CASE( "pool_task_complete_fires" )
{
        task_ctx ctx;
        bool     complete_fired = false;

        asrtio::test_pool_task t( *ctx.c, []( uint32_t, std::string_view ) {} );
        t.on_complete = [&] {
                complete_fired = true;
        };

        asrtio::task::res last = asrtio::task::runnning;
        ctx.spin_task( t );
        last = t.tick();  // one more tick — must stay finished
        CHECK( complete_fired );
        CHECK( last == asrtio::task::finished );
}

// ---------------------------------------------------------------------------
// uv_tasks queue ordering

TEST_CASE( "queue_fifo_order" )
{
        std::vector< int > order;

        // uv_tasks needs a live loop only for start(); we drive on_idle() directly
        uv_loop_t        loop{};
        asrtio::uv_tasks q( &loop );

        q.push( std::make_unique< asrtio::call_function_task >( [&] {
                order.push_back( 1 );
        } ) );
        q.push( std::make_unique< asrtio::call_function_task >( [&] {
                order.push_back( 2 );
        } ) );
        q.push( std::make_unique< asrtio::call_function_task >( [&] {
                order.push_back( 3 );
        } ) );

        q.on_idle();
        q.on_idle();
        q.on_idle();

        REQUIRE( order.size() == 3 );
        CHECK( order[0] == 1 );
        CHECK( order[1] == 2 );
        CHECK( order[2] == 3 );
}

TEST_CASE( "queue_one_per_idle" )
{
        int              count = 0;
        uv_loop_t        loop{};
        asrtio::uv_tasks q( &loop );

        q.push( std::make_unique< asrtio::call_function_task >( [&] {
                ++count;
        } ) );
        q.push( std::make_unique< asrtio::call_function_task >( [&] {
                ++count;
        } ) );

        q.on_idle();  // only first task executes
        CHECK( count == 1 );
}

TEST_CASE( "queue_pop_on_done" )
{
        uv_loop_t        loop{};
        asrtio::uv_tasks q( &loop );

        q.push( std::make_unique< asrtio::call_function_task >( [] {} ) );
        CHECK( q.tasks.size() == 1 );
        q.on_idle();
        CHECK( q.tasks.empty() );
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

struct suite_run
{
        recording_reporter reporter;
        bool               done = false;

        suite_run( uint32_t seed = 42 )
        {
                uv_loop_t* loop = uv_loop_new();

                auto rsim = asrtio::make_rsim( loop, seed );
                REQUIRE( rsim );
                rsim->start();

                auto sys = asrtio::make_tcp_sys( loop, "127.0.0.1", rsim->port() );
                REQUIRE( sys );

                asrtio::run_test_suite( *sys, reporter, [this, sys, rsim] {
                        reporter.done_names_at_on_done = (int) reporter.done_names.size();
                        sys->close();
                        rsim->close();
                        done = true;
                } );

                uv_run( loop, UV_RUN_DEFAULT );
                drain_loop( loop );
        }
};

TEST_CASE( "suite_basic" )
{
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
