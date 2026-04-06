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
#pragma once

/// @file demo.hpp
/// @brief Demo test types for the rsim simulator.
///
/// Each factory function returns a `demo_test` instance demonstrating a
/// specific testing pattern.  Register them with the reactor through
/// `asrtr::unit<demo_test>` exactly like any other test type.

#include "../asrtr/diag.h"
#include "../asrtr/record.h"
#include "../asrtrpp/diag.hpp"
#include "../asrtrpp/param.hpp"
#include "../asrtrpp/param_sender.hpp"
#include "../asrtrpp/collect.hpp"
#include "../asrtrpp/collect_sender.hpp"
#include "../asrtrpp/task_unit.hpp"

#include <functional>
#include <random>
#include <string>
#include <string_view>

namespace asrtio
{

struct demo_test;

struct demo_spec
{
        using body_fn = std::function< asrtr::status( demo_test&, asrtr::record& ) >;

        std::string tname;
        body_fn     body;
};

struct demo_test
{
        demo_spec            spec;
        asrtr::diag&         diag;
        asrtr::param_client& param;
        std::mt19937&        rng;
        int                  counter = 0;

        demo_test( demo_spec s, asrtr::diag& d, asrtr::param_client& p, std::mt19937& r )
          : spec( std::move( s ) )
          , diag( d )
          , param( p )
          , rng( r )
        {
        }

        char const* name()
        {
                return spec.tname.c_str();
        }

        asrtr::status operator()( asrtr::record& r )
        {
                return spec.body( *this, r );
        }
};

// ---------------------------------------------------------------------------
// Factory functions — each one demonstrates a different testing pattern.

/// Simplest possible test: immediately passes.
inline demo_spec make_demo_pass()
{
        return { .tname = "demo_pass", .body = []( demo_test&, asrtr::record& r ) -> asrtr::status {
                        r.state = ASRTR_TEST_PASS;
                        return ASRTR_SUCCESS;
                } };
}

/// Immediately fails and records a diagnostic message.
inline demo_spec make_demo_fail()
{
        return {
            .tname = "demo_fail", .body = []( demo_test& self, asrtr::record& r ) -> asrtr::status {
                    r.state = ASRTR_TEST_FAIL;
                    self.diag.record( "demo.hpp", __LINE__, "intentional" );
                    return ASRTR_SUCCESS;
            } };
}

/// Uses ASRTR_RECORD_CHECK with a passing condition.
/// Shows the soft-assertion macro in the success case.
inline demo_spec make_demo_check()
{
        return {
            .tname = "demo_check", .body = []( demo_test&, asrtr::record& r ) -> asrtr::status {
                    int sum = 2 + 3;
                    ASRTR_RECORD_CHECK( &r, sum == 5 );
                    if ( r.state != ASRTR_TEST_FAIL )
                            r.state = ASRTR_TEST_PASS;
                    return ASRTR_SUCCESS;
            } };
}

/// Uses ASRTR_RECORD_CHECK with a failing condition.
/// The check fails (soft failure — execution continues), and a diagnostic
/// is recorded so the controller can report the location.
inline demo_spec make_demo_check_fail()
{
        return {
            .tname = "demo_check_fail",
            .body  = []( demo_test& self, asrtr::record& r ) -> asrtr::status {
                    int sum = 2 + 2;
                    ASRTR_RECORD_CHECK( &r, sum == 5 );
                    if ( r.state == ASRTR_TEST_FAIL )
                            self.diag.record( "demo.hpp", __LINE__, "sum == 5" );
                    if ( r.state != ASRTR_TEST_FAIL )
                            r.state = ASRTR_TEST_PASS;
                    return ASRTR_SUCCESS;
            } };
}

/// Uses ASRTR_RECORD_REQUIRE with a failing condition.
/// The require macro fails and returns immediately — the lines after it
/// are never executed.
inline demo_spec make_demo_require_fail()
{
        return {
            .tname = "demo_require_fail",
            .body  = []( demo_test&, asrtr::record& r ) -> asrtr::status {
                    ASRTR_RECORD_REQUIRE( &r, 1 == 2 );
                    // Never reached — REQUIRE returned early.
                    r.state = ASRTR_TEST_PASS;
                    return ASRTR_SUCCESS;
            } };
}

/// Multi-step test: stays RUNNING for a few ticks, then passes.
/// Demonstrates the state-machine pattern where the test callback
/// is invoked repeatedly until it reports a final state.
inline demo_spec make_demo_counter()
{
        return {
            .tname = "demo_counter",
            .body  = []( demo_test& self, asrtr::record& r ) -> asrtr::status {
                    if ( self.counter < 3 ) {
                            ++self.counter;
                            r.state = ASRTR_TEST_RUNNING;
                    } else {
                            r.state = ASRTR_TEST_PASS;
                    }
                    return ASRTR_SUCCESS;
            } };
}

/// Nondeterministic test: randomly passes or fails based on the RNG seed.
/// Demonstrates seed-controlled randomisation — the same seed always
/// produces the same outcome.
inline demo_spec make_demo_random()
{
        return {
            .tname = "demo_random",
            .body  = []( demo_test& self, asrtr::record& r ) -> asrtr::status {
                    std::uniform_int_distribution< int > pick( 0, 2 );
                    static constexpr asrtr_test_state    k_results[] = {
                        ASRTR_TEST_PASS, ASRTR_TEST_FAIL, ASRTR_TEST_ERROR };
                    r.state = k_results[pick( self.rng )];
                    if ( r.state != ASRTR_TEST_PASS )
                            self.diag.record( "demo.hpp", __LINE__, "random outcome" );
                    return ASRTR_SUCCESS;
            } };
}

/// Nondeterministic multi-step test: runs for a random number of ticks
/// (1–6), then passes.  Combines the counter pattern with RNG-driven
/// duration.
inline demo_spec make_demo_random_counter()
{
        return {
            .tname = "demo_random_counter",
            .body  = []( demo_test& self, asrtr::record& r ) -> asrtr::status {
                    // On the first tick, pick a random target count.
                    if ( self.counter == 0 ) {
                            std::uniform_int_distribution< int > d( 1, 6 );
                            self.counter = -d( self.rng );  // negative = target
                    }
                    if ( self.counter < 0 ) {
                            ++self.counter;
                            r.state = ASRTR_TEST_RUNNING;
                            if ( self.counter == 0 )
                                    r.state = ASRTR_TEST_PASS;
                    }
                    return ASRTR_SUCCESS;
            } };
}

// ---------------------------------------------------------------------------
// Param-aware demo tests — demonstrate reading parameters from the tree.

namespace detail
{

struct param_qr
{
        asrtl::flat_id    first_child = 0;
        uint32_t          u32_val     = 0;
        asrtl::flat_id    next_sib    = 0;
        asrtr_param_query q           = {};
};

inline void param_obj_qr_cb( asrtr_param_client*, asrtr_param_query* qq, asrtl_flat_child_list val )
{
        auto& s = *static_cast< param_qr* >( qq->cb_ptr );
        if ( qq->error_code == 0 ) {
                s.first_child = val.first_child;
                s.next_sib    = qq->next_sibling;
        }
}

inline void param_u32_qr_cb( asrtr_param_client*, asrtr_param_query* qq, uint32_t val )
{
        auto& s = *static_cast< param_qr* >( qq->cb_ptr );
        if ( qq->error_code == 0 ) {
                s.u32_val  = val;
                s.next_sib = qq->next_sibling;
        }
}

inline void param_any_qr_cb( asrtr_param_client*, asrtr_param_query* qq, asrtl_flat_value )
{
        auto& s = *static_cast< param_qr* >( qq->cb_ptr );
        if ( qq->error_code == 0 )
                s.next_sib = qq->next_sibling;
}

}  // namespace detail

/// Reads the first u32 child of the param root object and checks > 0.
/// If no param tree is loaded, passes trivially.
/// Demonstrates reading a single param value.
inline demo_spec make_demo_param_value()
{
        return {
            .tname = "demo_param_value",
            .body  = [pq = detail::param_qr{}](
                        demo_test& self, asrtr::record& r ) mutable -> asrtr::status {
                    if ( r.state == ASRTR_TEST_INIT ) {
                            self.counter = 0;
                            pq           = {};
                    }
                    if ( self.param.query_pending() ) {
                            r.state = ASRTR_TEST_RUNNING;
                            return ASRTR_SUCCESS;
                    }
                    switch ( self.counter ) {
                    case 0:
                            if ( !self.param.ready() ) {
                                    r.state = ASRTR_TEST_PASS;
                                    return ASRTR_SUCCESS;
                            }
                            pq = {};
                            if ( auto s = self.param.fetch< asrtl::obj >(
                                     &pq.q, self.param.root_id(), detail::param_obj_qr_cb, &pq );
                                 s != ASRTL_SUCCESS ) {
                                    self.diag.record( "demo.hpp", __LINE__, "fetch failed" );
                                    r.state = ASRTR_TEST_FAIL;
                                    return ASRTR_SUCCESS;
                            }
                            self.counter = 1;
                            r.state      = ASRTR_TEST_RUNNING;
                            return ASRTR_SUCCESS;
                    case 1: {
                            ASRTR_RECORD_REQUIRE( &r, pq.q.error_code == 0 );
                            auto first = pq.first_child;
                            pq = {};
                            if ( auto s = self.param.fetch< uint32_t >(
                                     &pq.q, first, detail::param_u32_qr_cb, &pq );
                                 s != ASRTL_SUCCESS ) {
                                    self.diag.record( "demo.hpp", __LINE__, "fetch failed" );
                                    r.state = ASRTR_TEST_FAIL;
                                    return ASRTR_SUCCESS;
                            }
                            self.counter = 2;
                            r.state      = ASRTR_TEST_RUNNING;
                            return ASRTR_SUCCESS;
                    }
                    case 2:
                            ASRTR_RECORD_CHECK( &r, pq.q.error_code == 0 );
                            if ( r.state == ASRTR_TEST_FAIL ) {
                                    self.diag.record( "demo.hpp", __LINE__, "expected u32" );
                            } else {
                                    ASRTR_RECORD_CHECK( &r, pq.u32_val > 0 );
                                    if ( r.state == ASRTR_TEST_FAIL )
                                            self.diag.record( "demo.hpp", __LINE__, "val > 0" );
                                    else
                                            r.state = ASRTR_TEST_PASS;
                            }
                            return ASRTR_SUCCESS;
                    }
                    return ASRTR_SUCCESS;
            } };
}

/// Counts children of the param root object. Passes if count > 0.
/// Demonstrates walking the sibling chain.
inline demo_spec make_demo_param_count()
{
        return {
            .tname = "demo_param_count",
            .body  = [pq = detail::param_qr{}, child_count = 0](
                        demo_test& self, asrtr::record& r ) mutable -> asrtr::status {
                    if ( r.state == ASRTR_TEST_INIT ) {
                            self.counter = 0;
                            child_count  = 0;
                            pq           = {};
                    }
                    if ( self.param.query_pending() ) {
                            r.state = ASRTR_TEST_RUNNING;
                            return ASRTR_SUCCESS;
                    }
                    switch ( self.counter ) {
                    case 0:
                            if ( !self.param.ready() ) {
                                    r.state = ASRTR_TEST_PASS;
                                    return ASRTR_SUCCESS;
                            }
                            pq = {};
                            if ( auto s = self.param.fetch< asrtl::obj >(
                                     &pq.q, self.param.root_id(), detail::param_obj_qr_cb, &pq );
                                 s != ASRTL_SUCCESS ) {
                                    self.diag.record( "demo.hpp", __LINE__, "fetch failed" );
                                    r.state = ASRTR_TEST_FAIL;
                                    return ASRTR_SUCCESS;
                            }
                            self.counter = 1;
                            r.state      = ASRTR_TEST_RUNNING;
                            return ASRTR_SUCCESS;
                    case 1: {
                            ASRTR_RECORD_REQUIRE( &r, pq.q.error_code == 0 );
                            auto first = pq.first_child;
                            pq = {};
                            if ( auto s = self.param.fetch< void >(
                                     &pq.q, first, detail::param_any_qr_cb, &pq );
                                 s != ASRTL_SUCCESS ) {
                                    self.diag.record( "demo.hpp", __LINE__, "fetch failed" );
                                    r.state = ASRTR_TEST_FAIL;
                                    return ASRTR_SUCCESS;
                            }
                            self.counter = 2;
                            r.state      = ASRTR_TEST_RUNNING;
                            return ASRTR_SUCCESS;
                    }
                    case 2:
                            ++child_count;
                            if ( pq.next_sib != 0 ) {
                                    auto next = pq.next_sib;
                                    pq = {};
                                    if ( auto s = self.param.fetch< void >(
                                             &pq.q, next, detail::param_any_qr_cb, &pq );
                                         s != ASRTL_SUCCESS ) {
                                            self.diag.record(
                                                "demo.hpp", __LINE__, "fetch failed" );
                                            r.state = ASRTR_TEST_FAIL;
                                            return ASRTR_SUCCESS;
                                    }
                                    r.state = ASRTR_TEST_RUNNING;
                                    return ASRTR_SUCCESS;
                            }
                            ASRTR_RECORD_CHECK( &r, child_count > 0 );
                            if ( r.state == ASRTR_TEST_FAIL )
                                    self.diag.record( "demo.hpp", __LINE__, "child_count > 0" );
                            else
                                    r.state = ASRTR_TEST_PASS;
                            return ASRTR_SUCCESS;
                    }
                    return ASRTR_SUCCESS;
            } };
}

/// Finds a child by key in the param root object and checks > 0.
/// Demonstrates the find-by-key API for direct key lookup.
inline demo_spec make_demo_param_find()
{
        return {
            .tname = "demo_param_find",
            .body  = [pq = detail::param_qr{}](
                        demo_test& self, asrtr::record& r ) mutable -> asrtr::status {
                    if ( r.state == ASRTR_TEST_INIT ) {
                            self.counter = 0;
                            pq           = {};
                    }
                    if ( self.param.query_pending() ) {
                            r.state = ASRTR_TEST_RUNNING;
                            return ASRTR_SUCCESS;
                    }
                    switch ( self.counter ) {
                    case 0:
                            if ( !self.param.ready() ) {
                                    r.state = ASRTR_TEST_PASS;
                                    return ASRTR_SUCCESS;
                            }
                            pq = {};
                            if ( auto s = self.param.find< uint32_t >(
                                     &pq.q,
                                     self.param.root_id(),
                                     "count",
                                     detail::param_u32_qr_cb,
                                     &pq );
                                 s != ASRTL_SUCCESS ) {
                                    self.diag.record( "demo.hpp", __LINE__, "find failed" );
                                    r.state = ASRTR_TEST_FAIL;
                                    return ASRTR_SUCCESS;
                            }
                            self.counter = 1;
                            r.state      = ASRTR_TEST_RUNNING;
                            return ASRTR_SUCCESS;
                    case 1:
                            ASRTR_RECORD_CHECK( &r, pq.q.error_code == 0 );
                            if ( r.state == ASRTR_TEST_FAIL ) {
                                    self.diag.record( "demo.hpp", __LINE__, "find 'count'" );
                            } else {
                                    ASRTR_RECORD_CHECK( &r, pq.u32_val > 0 );
                                    if ( r.state == ASRTR_TEST_FAIL )
                                            self.diag.record( "demo.hpp", __LINE__, "count > 0" );
                                    else
                                            r.state = ASRTR_TEST_PASS;
                            }
                            return ASRTR_SUCCESS;
                    }
                    return ASRTR_SUCCESS;
            } };
}


/// Immediately passes — simplest possible coroutine test.
struct pass_demo_task : asrtr::task_test
{
        char const* name = "pass_demo_task";

        asrtr::task< void > exec()
        {
                co_return;
        }
};

/// Immediately fails via test_fail.
struct fail_demo_task : asrtr::task_test
{
        char const* name = "fail_demo_task";

        asrtr::task< void > exec()
        {
                co_yield asrtr::with_error{ asrtr::test_fail };
        }
};

/// Reports a test infrastructure error via test_error.
struct error_demo_task : asrtr::task_test
{
        char const* name = "error_demo_task";

        asrtr::task< void > exec()
        {
                co_yield asrtr::with_error{ asrtr::test_error };
        }
};

/// Multi-step pass: suspends 3 times, then completes.
/// Coroutine equivalent of the demo_counter state machine.
struct counter_demo_task : asrtr::task_test
{
        char const* name = "counter_demo_task";

        asrtr::task< void > exec()
        {
                for ( int i = 0; i < 3; ++i )
                        co_await asrtr::suspend;
        }
};

/// Checks a passing condition, then completes successfully.
struct check_demo_task : asrtr::task_test
{
        char const* name = "check_demo_task";

        asrtr::task< void > exec()
        {
                if ( 2 + 3 != 5 )
                        co_yield asrtr::with_error{ asrtr::test_fail };
        }
};

/// Checks a failing condition — yields test_fail.
struct check_fail_demo_task : asrtr::task_test
{
        char const* name = "check_fail_demo_task";

        asrtr::task< void > exec()
        {
                if ( 2 + 2 != 5 )
                        co_yield asrtr::with_error{ asrtr::test_fail };
        }
};

/// Suspends several times, then fails — failure after async work.
struct multi_step_fail_demo_task : asrtr::task_test
{
        char const* name = "multi_step_fail_demo_task";

        asrtr::task< void > exec()
        {
                for ( int i = 0; i < 3; ++i )
                        co_await asrtr::suspend;
                co_yield asrtr::with_error{ asrtr::test_fail };
        }
};


/// Parameter integration into coroutine
struct param_query_demo_task : asrtr::task_test
{
        char const*          name = "param_query_demo_task";
        asrtr::param_client& pc;

        param_query_demo_task( asrtr::task_ctx& ctx, asrtr::param_client& p )
          : task_test( ctx )
          , pc( p )
        {
        }

        asrtr::task< void > exec()
        {
                // Query param node with ID 1
                auto x = co_await asrtr::fetch< uint32_t >( pc, 1 );
                auto y = co_await asrtr::fetch< int32_t >( pc, 1 );
                if ( x != y )
                        co_yield asrtr::with_error{ asrtr::test_fail };
        }
};


/// Search by name in object - coroutine
struct param_query_find_demo_task : asrtr::task_test
{
        char const*          name = "param_query_find_demo_task";
        asrtr::param_client& pc;

        param_query_find_demo_task( asrtr::task_ctx& ctx, asrtr::param_client& p )
          : task_test( ctx )
          , pc( p )
        {
        }

        asrtr::task< void > exec()
        {
                // Query param node with key "count" under root
                auto x = co_await asrtr::find< uint32_t >( pc, pc.root_id(), "count" );
                auto y = co_await asrtr::find< int32_t >( pc, pc.root_id(), "count" );
                if ( x != y )
                        co_yield asrtr::with_error{ asrtr::test_fail };
        }
};


/// Type overview: fetches every supported param type by key.
/// The param tree is expected to have children under root with keys:
///   "u32" (uint32_t), "i32" (int32_t), "flt" (float), "str" (string),
///   "bln" (bool), "obj" (object), "arr" (array).
struct param_type_overview_task : asrtr::task_test
{
        char const*          name = "param_type_overview_task";
        asrtr::param_client& pc;

        param_type_overview_task( asrtr::task_ctx& ctx, asrtr::param_client& p )
          : task_test( ctx )
          , pc( p )
        {
        }

        asrtr::task< void > exec()
        {
                auto root = pc.root_id();

                auto u = co_await asrtr::find< uint32_t >( pc, root, "u32" );
                if ( u != 42 )
                        co_yield asrtr::with_error{ asrtr::test_fail };

                auto i = co_await asrtr::find< int32_t >( pc, root, "i32" );
                if ( i != -7 )
                        co_yield asrtr::with_error{ asrtr::test_fail };

                auto f = co_await asrtr::find< float >( pc, root, "flt" );
                if ( f < 3.13f || f > 3.15f )
                        co_yield asrtr::with_error{ asrtr::test_fail };

                auto s = co_await asrtr::find< char const* >( pc, root, "str" );
                if ( std::string_view{ s } != "hello" )
                        co_yield asrtr::with_error{ asrtr::test_fail };

                auto b = co_await asrtr::find< bool >( pc, root, "bln" );
                if ( !b )
                        co_yield asrtr::with_error{ asrtr::test_fail };

                asrtl_flat_child_list obj =
                    co_await asrtr::find< asrtl::obj >( pc, root, "obj" );
                if ( obj.first_child == 0 )
                        co_yield asrtr::with_error{ asrtr::test_fail };

                asrtl_flat_child_list arr =
                    co_await asrtr::find< asrtl::arr >( pc, root, "arr" );
                if ( arr.first_child == 0 )
                        co_yield asrtr::with_error{ asrtr::test_fail };

                // Iterate over array elements
                uint32_t          count = 0;
                asrtl::flat_id    id    = arr.first_child;
                while ( id != 0 ) {
                        auto [val, key, next_sibling] =
                            co_await asrtr::fetch< uint32_t >( pc, id );
                        ++count;
                        id = next_sibling;
                }
                if ( count == 0 )
                        co_yield asrtr::with_error{ asrtr::test_fail };

                // Iterate over object children and touch their keys
                count = 0;
                id    = obj.first_child;
                while ( id != 0 ) {
                        auto [val, key, next_sibling] =
                            co_await asrtr::fetch< asrtl_flat_value >( pc, id );
                        if ( !key || key[0] == '\0' )
                                co_yield asrtr::with_error{ asrtr::test_fail };
                        ++count;
                        id = next_sibling;
                }
                if ( count == 0 )
                        co_yield asrtr::with_error{ asrtr::test_fail };
        }
};


/// Collector demo: appends a small tree via the collect channel.
/// Produces: root OBJECT → "value" U32=42, "tag" STR="demo"
struct collect_demo_task : asrtr::task_test
{
        char const*           name = "collect_demo_task";
        asrtr::collect_client& cc;

        collect_demo_task( asrtr::task_ctx& ctx, asrtr::collect_client& c )
          : task_test( ctx )
          , cc( c )
        {
        }

        asrtr::task< void > exec()
        {
                auto root = cc.root_id();
                auto obj  = co_await asrtr::append< asrtl::obj >( cc, root );
                co_await asrtr::append( cc, obj, "value", 42u );
                co_await asrtr::append( cc, obj, "tag", "demo" );
        }
};


}  // namespace asrtio
