// pbar_demo.cpp — visual integration demo for pbar.hpp
//
// Run:  ./pbar_demo [--total N] [--fail-rate N] [--delay N] [--fast]
//
// Simulates a test runner so you can see the bar, status line and scrolling
// log in action.

#include "../asrtio/deps/pbar.hpp"

#include <CLI/CLI.hpp>
#include <chrono>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace std::literals::chrono_literals;

static std::vector< std::string > const k_names = {
    "test_allocator_basic",   "test_allocator_overflow",   "test_allocator_free_null",
    "test_controller_init",   "test_controller_dispatch",  "test_controller_teardown",
    "test_callbacks_timeout", "test_callbacks_fire_order", "test_reactor_create",
    "test_reactor_dispatch",  "test_reactor_close",        "test_reactor_double_close",
    "test_chann_write",       "test_chann_read",           "test_chann_full",
    "test_cobs_encode",       "test_cobs_decode",          "test_cobs_roundtrip",
    "test_cobs_empty",        "test_status_to_str",        "test_ecode_to_str",
    "test_source_to_str",     "test_span_empty",           "test_span_slice",
    "test_span_out_of_range", "test_util_clamp",           "test_util_min_max",
    "test_util_round_trip",
};

int main( int argc, char** argv )
{
        int  total         = 20;
        int  fail_rate_pct = 15;
        int  delay_ms      = 80;
        bool fast          = false;

        CLI::App app{ "pbar_demo — visual progress bar demo" };
        app.add_option( "--total", total, "Number of tests to simulate" );
        app.add_option( "--fail-rate", fail_rate_pct, "Percentage of tests that fail (0–100)" );
        app.add_option( "--delay", delay_ms, "Delay between tests in ms" );
        app.add_flag( "--fast", fast, "Use 1 ms delays (useful for CI)" );
        CLI11_PARSE( app, argc, argv );

        if ( fast )
                delay_ms = 1;

        std::mt19937                             rng( 42 );
        std::uniform_int_distribution< int >     fail_roll( 0, 99 );
        std::uniform_real_distribution< double > dur_dist( 0.2, 180.0 );

        pbar::bar_config cfg;
        cfg.total       = total;
        cfg.suite_label = "assert_suite";

        pbar::terminal_progress bar( cfg );

        int passed = 0;
        int failed = 0;

        for ( int i = 0; i < total; ++i ) {
                std::string const& name = k_names[i % k_names.size()];

                bar.set_status(
                    pbar::fg( "Running: ", pbar::colors::dim_gray ) + pbar::bold( name ) );

                std::this_thread::sleep_for( std::chrono::milliseconds( delay_ms ) );

                double dur_ms = dur_dist( rng );
                bool   ok     = ( fail_roll( rng ) >= fail_rate_pct );
                if ( ok )
                        ++passed;
                else
                        ++failed;

                bar.log_result( name, ok, dur_ms );
                bar.set_progress( i + 1, failed );
        }

        // ── Final summary in the status line ────────────────────────────────
        std::string summary =
            pbar::bold( "Done  " ) +
            pbar::fg( std::to_string( passed ) + " passed", pbar::colors::green ) +
            pbar::dim( "  /  " ) +
            ( failed > 0 ? pbar::fg( std::to_string( failed ) + " failed", pbar::colors::red ) :
                           pbar::dim( "0 failed" ) ) +
            pbar::dim( "  |  " ) +
            pbar::fg( std::to_string( total ) + " total", pbar::colors::white );

        bar.set_status( summary );
        bar.set_progress( total, failed );
        bar.finish();

        return failed > 0 ? 1 : 0;
}
