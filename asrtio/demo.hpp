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

#include <functional>
#include <random>
#include <string>

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
        bool             got      = false;
        asrtl_flat_value value    = {};
        asrtl_flat_id    next_sib = 0;
};

inline void param_qr_cb(
    void*            ptr,
    asrtl_flat_id,
    char const*,
    asrtl_flat_value value,
    asrtl_flat_id    next_sib )
{
        auto& s    = *static_cast< param_qr* >( ptr );
        s.got      = true;
        s.value    = value;
        s.next_sib = next_sib;
}

inline void param_qr_err( void* ptr, uint8_t, asrtl_flat_id )
{
        auto& s = *static_cast< param_qr* >( ptr );
        s.got   = true;
        s.value = {};
}

}  // namespace detail

/// Reads the first u32 child of the param root object and checks > 0.
/// If no param tree is loaded, passes trivially.
/// Demonstrates reading a single param value.
inline demo_spec make_demo_param_value()
{
        return {
            .tname = "demo_param_value",
            .body  = [pq = detail::param_qr{}]( demo_test& self,
                                                 asrtr::record& r ) mutable -> asrtr::status {
                    if ( r.state == ASRTR_TEST_INIT ) {
                            self.counter = 0;
                            pq           = {};
                    }
                    switch ( self.counter ) {
                    case 0:
                            if ( !self.param.ready() ) {
                                    r.state = ASRTR_TEST_PASS;
                                    return ASRTR_SUCCESS;
                            }
                            pq = {};
                            std::ignore = self.param.query(
                                self.param.root_id(),
                                detail::param_qr_cb,
                                &pq,
                                detail::param_qr_err,
                                &pq );
                            self.counter = 1;
                            r.state      = ASRTR_TEST_RUNNING;
                            return ASRTR_SUCCESS;
                    case 1: {
                            if ( !pq.got ) {
                                    r.state = ASRTR_TEST_RUNNING;
                                    return ASRTR_SUCCESS;
                            }
                            ASRTR_RECORD_REQUIRE(
                                &r, pq.value.type == ASRTL_FLAT_VALUE_TYPE_OBJECT );
                            auto first   = pq.value.obj_val.first_child;
                            pq           = {};
                            std::ignore = self.param.query(
                                first,
                                detail::param_qr_cb,
                                &pq,
                                detail::param_qr_err,
                                &pq );
                            self.counter = 2;
                            r.state      = ASRTR_TEST_RUNNING;
                            return ASRTR_SUCCESS;
                    }
                    case 2:
                            if ( !pq.got ) {
                                    r.state = ASRTR_TEST_RUNNING;
                                    return ASRTR_SUCCESS;
                            }
                            ASRTR_RECORD_CHECK(
                                &r, pq.value.type == ASRTL_FLAT_VALUE_TYPE_U32 );
                            if ( r.state == ASRTR_TEST_FAIL ) {
                                    self.diag.record( "demo.hpp", __LINE__, "expected u32" );
                            } else {
                                    ASRTR_RECORD_CHECK( &r, pq.value.u32_val > 0 );
                                    if ( r.state == ASRTR_TEST_FAIL )
                                            self.diag.record(
                                                "demo.hpp", __LINE__, "val > 0" );
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
                    switch ( self.counter ) {
                    case 0:
                            if ( !self.param.ready() ) {
                                    r.state = ASRTR_TEST_PASS;
                                    return ASRTR_SUCCESS;
                            }
                            pq = {};
                            std::ignore = self.param.query(
                                self.param.root_id(),
                                detail::param_qr_cb,
                                &pq,
                                detail::param_qr_err,
                                &pq );
                            self.counter = 1;
                            r.state      = ASRTR_TEST_RUNNING;
                            return ASRTR_SUCCESS;
                    case 1: {
                            if ( !pq.got ) {
                                    r.state = ASRTR_TEST_RUNNING;
                                    return ASRTR_SUCCESS;
                            }
                            ASRTR_RECORD_REQUIRE(
                                &r, pq.value.type == ASRTL_FLAT_VALUE_TYPE_OBJECT );
                            auto first   = pq.value.obj_val.first_child;
                            pq           = {};
                            std::ignore = self.param.query(
                                first,
                                detail::param_qr_cb,
                                &pq,
                                detail::param_qr_err,
                                &pq );
                            self.counter = 2;
                            r.state      = ASRTR_TEST_RUNNING;
                            return ASRTR_SUCCESS;
                    }
                    case 2:
                            if ( !pq.got ) {
                                    r.state = ASRTR_TEST_RUNNING;
                                    return ASRTR_SUCCESS;
                            }
                            ++child_count;
                            if ( pq.next_sib != 0 ) {
                                    auto next = pq.next_sib;
                                    pq        = {};
                                    std::ignore = self.param.query(
                                        next,
                                        detail::param_qr_cb,
                                        &pq,
                                        detail::param_qr_err,
                                        &pq );
                                    r.state = ASRTR_TEST_RUNNING;
                                    return ASRTR_SUCCESS;
                            }
                            ASRTR_RECORD_CHECK( &r, child_count > 0 );
                            if ( r.state == ASRTR_TEST_FAIL )
                                    self.diag.record(
                                        "demo.hpp", __LINE__, "child_count > 0" );
                            else
                                    r.state = ASRTR_TEST_PASS;
                            return ASRTR_SUCCESS;
                    }
                    return ASRTR_SUCCESS;
            } };
}

}  // namespace asrtio
