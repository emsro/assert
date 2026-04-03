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
#include "../asrtcpp/param.hpp"
#include "../asrtl/log.h"
#include "../asrtlpp/util.hpp"
#include "../asrtr/reactor.h"
#include "../asrtr/record.h"
#include "../asrtrpp/diag.hpp"
#include "../asrtrpp/fmt.hpp"
#include "../asrtrpp/param.hpp"
#include "../asrtrpp/reactor.hpp"
#include "./collector.hpp"
#include "./util.h"

#include <algorithm>
#include <doctest/doctest.h>
#include <format>
#include <functional>
#include <span>
#include <vector>

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

// ---------------------------------------------------------------------------
// param_client wrapper — loopback with C++ param_server

static std::vector< uint8_t > flatten( asrtl::rec_span const* buff )
{
        std::vector< uint8_t > v;
        for ( auto const* seg = buff; seg; seg = seg->next )
                v.insert( v.end(), seg->b, seg->e );
        return v;
}

struct param_loopback_cpp_ctx
{
        asrtl_node srv_head = {};
        asrtl_node cli_head = {};

        // cross-wired senders
        std::function< asrtl_status( asrtl_chann_id, asrtl_rec_span* ) > srv_send{
            [this]( asrtl_chann_id, asrtl_rec_span* buff ) {
                    auto flat = flatten( buff );
                    auto sp   = asrtl::cnv( std::span{ flat } );
                    cli.node()->recv_cb( cli.node()->recv_ptr, sp );
                    return ASRTL_SUCCESS;
            } };

        std::function< asrtl_status( asrtl_chann_id, asrtl_rec_span* ) > cli_send{
            [this]( asrtl_chann_id, asrtl_rec_span* buff ) {
                    auto flat = flatten( buff );
                    auto sp   = asrtl::cnv( std::span{ flat } );
                    srv.node()->recv_cb( srv.node()->recv_ptr, sp );
                    return ASRTL_SUCCESS;
            } };

        asrtc::param_server srv{ &srv_head, srv_send, asrtl_default_allocator() };

        static constexpr uint32_t BUF_SZ          = 256;
        uint8_t                   cli_buf[BUF_SZ] = {};
        asrtr::param_client       cli{
            &cli_head,
            cli_send,
            asrtl_span{ .b = cli_buf, .e = cli_buf + BUF_SZ },
            100 };

        // response state
        struct received_node
        {
                asrtl_flat_id    id;
                std::string      key;
                asrtl_flat_value value;
                asrtl_flat_id    next_sibling;
        };
        std::vector< received_node > received;
        int                          error_called = 0;
        uint32_t                     t            = 1;
        asrtr_param_query            query        = {};

        static void query_cb( asrtr_param_client*, asrtr_param_query* q, asrtl_flat_value val )
        {
                auto* ctx = (param_loopback_cpp_ctx*) q->cb_ptr;
                if ( q->error_code != 0 ) {
                        ctx->error_called++;
                } else {
                        ctx->received.push_back(
                            { q->node_id, q->key ? q->key : "", val, q->next_sibling } );
                }
        }

        void spin( int max_iter = 100 )
        {
                for ( int i = 0; i < max_iter; i++ ) {
                        srv.tick( t++ );
                        cli.tick();
                        if ( cli.ready() )
                                break;
                }
        }

        void spin_query( int max_iter = 100 )
        {
                for ( int i = 0; i < max_iter; i++ ) {
                        srv.tick( t++ );
                        cli.tick();
                }
        }

        param_loopback_cpp_ctx()
        {
                srv_head.chid = ASRTL_CORE;
                cli_head.chid = ASRTL_CORE;
        }
};

TEST_CASE_FIXTURE( param_loopback_cpp_ctx, "param_cpp_loopback_handshake" )
{
        struct asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        srv.set_tree( &tree );

        CHECK_FALSE( cli.ready() );
        auto noop = [] {};
        CHECK_EQ( ASRTL_SUCCESS, srv.send_ready( 1u, noop, 1000 ) );
        spin();
        CHECK( cli.ready() );
        CHECK_EQ( 1u, cli.root_id() );

        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_loopback_cpp_ctx, "param_cpp_loopback_traversal" )
{
        struct asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "a", asrtl_flat_value_u32( 10 ) );
        asrtl_flat_tree_append( &tree, 1, 3, "b", asrtl_flat_value_str( "hi" ) );
        srv.set_tree( &tree );

        auto noop = [] {};
        CHECK_EQ( ASRTL_SUCCESS, srv.send_ready( 1u, noop, 1000 ) );
        spin();
        REQUIRE( cli.ready() );

        // Query root
        CHECK_EQ( ASRTL_SUCCESS, cli.query( &query, 1u, query_cb, this ) );
        spin_query();
        REQUIRE_EQ( 1u, received.size() );
        CHECK_EQ( 1u, received[0].id );
        CHECK_EQ( (uint8_t) ASRTL_FLAT_VALUE_TYPE_OBJECT, (uint8_t) received[0].value.type );
        asrtl_flat_id first_child = received[0].value.obj_val.first_child;

        // Query first child "a"
        CHECK_EQ( ASRTL_SUCCESS, cli.query( &query, first_child, query_cb, this ) );
        spin_query();
        REQUIRE_EQ( 2u, received.size() );
        CHECK_EQ( "a", received[1].key );
        CHECK_EQ( 10u, received[1].value.u32_val );
        asrtl_flat_id next_sib = received[1].next_sibling;

        // Query next sibling "b"
        CHECK_EQ( ASRTL_SUCCESS, cli.query( &query, next_sib, query_cb, this ) );
        spin_query();
        REQUIRE_EQ( 3u, received.size() );
        CHECK_EQ( "b", received[2].key );
        CHECK_EQ( (uint8_t) ASRTL_FLAT_VALUE_TYPE_STR, (uint8_t) received[2].value.type );
        CHECK_EQ( 0u, received[2].next_sibling );

        CHECK_EQ( 0, error_called );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_loopback_cpp_ctx, "param_cpp_error_reaches_callback" )
{
        struct asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        srv.set_tree( &tree );

        auto noop = [] {};
        CHECK_EQ( ASRTL_SUCCESS, srv.send_ready( 1u, noop, 1000 ) );
        spin();
        REQUIRE( cli.ready() );

        // Query non-existent node — server sends ERROR
        CHECK_EQ( ASRTL_SUCCESS, cli.query( &query, 999u, query_cb, this ) );
        spin_query();

        // The node doesn't exist, so server sends a response with NONE type
        // (not an error — the server encodes whatever the tree returns).
        // The callback fires with the queried id.
        REQUIRE_EQ( 1u, received.size() );
        CHECK_EQ( 999u, received[0].id );

        asrtl_flat_tree_deinit( &tree );
}

// ---------------------------------------------------------------------------
// typed query<T> tests

struct typed_loopback_ctx
{
        asrtl_node srv_head = {};
        asrtl_node cli_head = {};

        std::function< asrtl_status( asrtl_chann_id, asrtl_rec_span* ) > srv_send{
            [this]( asrtl_chann_id, asrtl_rec_span* buff ) {
                    auto flat = flatten( buff );
                    auto sp   = asrtl::cnv( std::span{ flat } );
                    cli.node()->recv_cb( cli.node()->recv_ptr, sp );
                    return ASRTL_SUCCESS;
            } };

        std::function< asrtl_status( asrtl_chann_id, asrtl_rec_span* ) > cli_send{
            [this]( asrtl_chann_id, asrtl_rec_span* buff ) {
                    auto flat = flatten( buff );
                    auto sp   = asrtl::cnv( std::span{ flat } );
                    srv.node()->recv_cb( srv.node()->recv_ptr, sp );
                    return ASRTL_SUCCESS;
            } };

        asrtc::param_server srv{ &srv_head, srv_send, asrtl_default_allocator() };

        static constexpr uint32_t BUF_SZ          = 256;
        uint8_t                   cli_buf[BUF_SZ] = {};
        asrtr::param_client       cli{
            &cli_head,
            cli_send,
            asrtl_span{ .b = cli_buf, .e = cli_buf + BUF_SZ },
            100 };

        uint32_t          t     = 1;
        asrtr_param_query query = {};

        // results
        uint32_t              u32_val = 0;
        int32_t               i32_val = 0;
        float                 flt_val = 0.0f;
        std::string           str_val;
        asrtl_flat_child_list obj_val  = {};
        asrtl_flat_child_list arr_val  = {};
        uint32_t              bool_val = 0;
        int                   cb_count = 0;
        bool                  got_null = false;

        typed_loopback_ctx()
        {
                srv_head.chid = ASRTL_CORE;
                cli_head.chid = ASRTL_CORE;
        }

        void setup_tree_and_handshake( asrtl_flat_tree* tree )
        {
                srv.set_tree( tree );
                auto noop = [] {};
                CHECK_EQ( ASRTL_SUCCESS, srv.send_ready( 1u, noop, 1000 ) );
                for ( int i = 0; i < 100; i++ ) {
                        srv.tick( t++ );
                        cli.tick();
                        if ( cli.ready() )
                                break;
                }
                REQUIRE( cli.ready() );
        }

        void spin_query( int max_iter = 100 )
        {
                for ( int i = 0; i < max_iter; i++ ) {
                        srv.tick( t++ );
                        cli.tick();
                }
        }
};

TEST_CASE_FIXTURE( typed_loopback_ctx, "typed_query_u32_happy" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "val", asrtl_flat_value_u32( 42 ) );
        setup_tree_and_handshake( &tree );

        auto cb = [this]( asrtr_param_client*, asrtr_param_query*, uint32_t v ) {
                cb_count++;
                u32_val = v;
        };
        CHECK_EQ( ASRTL_SUCCESS, cli.query< uint32_t >( &query, 2u, cb ) );
        spin_query();
        CHECK_EQ( 1, cb_count );
        CHECK_EQ( 42u, u32_val );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( typed_loopback_ctx, "typed_query_u32_mismatch" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "val", asrtl_flat_value_str( "nope" ) );
        setup_tree_and_handshake( &tree );

        auto cb = [this]( asrtr_param_client*, asrtr_param_query* q, uint32_t ) {
                cb_count++;
                got_null = ( q->error_code != 0 );
        };
        CHECK_EQ( ASRTL_SUCCESS, cli.query< uint32_t >( &query, 2u, cb ) );
        spin_query();
        CHECK_EQ( 1, cb_count );
        CHECK( got_null );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( typed_loopback_ctx, "typed_query_i32_happy" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "val", asrtl_flat_value_i32( -7 ) );
        setup_tree_and_handshake( &tree );

        auto cb = [this]( asrtr_param_client*, asrtr_param_query*, int32_t v ) {
                cb_count++;
                i32_val = v;
        };
        CHECK_EQ( ASRTL_SUCCESS, cli.query< int32_t >( &query, 2u, cb ) );
        spin_query();
        CHECK_EQ( 1, cb_count );
        CHECK_EQ( -7, i32_val );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( typed_loopback_ctx, "typed_query_str_happy" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "val", asrtl_flat_value_str( "hello" ) );
        setup_tree_and_handshake( &tree );

        auto cb = [this]( asrtr_param_client*, asrtr_param_query*, char const* v ) {
                cb_count++;
                if ( v )
                        str_val = v;
        };
        CHECK_EQ( ASRTL_SUCCESS, cli.query< char const* >( &query, 2u, cb ) );
        spin_query();
        CHECK_EQ( 1, cb_count );
        CHECK_EQ( "hello", str_val );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( typed_loopback_ctx, "typed_query_float_happy" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "val", asrtl_flat_value_float( 3.14f ) );
        setup_tree_and_handshake( &tree );

        auto cb = [this]( asrtr_param_client*, asrtr_param_query*, float v ) {
                cb_count++;
                flt_val = v;
        };
        CHECK_EQ( ASRTL_SUCCESS, cli.query< float >( &query, 2u, cb ) );
        spin_query();
        CHECK_EQ( 1, cb_count );
        CHECK_EQ( doctest::Approx( 3.14f ), flt_val );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( typed_loopback_ctx, "typed_query_any_happy" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "val", asrtl_flat_value_u32( 99 ) );
        setup_tree_and_handshake( &tree );

        auto cb = [this]( asrtr_param_client*, asrtr_param_query*, asrtl_flat_value v ) {
                cb_count++;
                u32_val = v.u32_val;
        };
        // untyped query — explicit asrtl_flat_value
        CHECK_EQ( ASRTL_SUCCESS, cli.query< asrtl_flat_value >( &query, 2u, cb ) );
        spin_query();
        CHECK_EQ( 1, cb_count );
        CHECK_EQ( 99u, u32_val );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( typed_loopback_ctx, "query_pending_cpp" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "val", asrtl_flat_value_u32( 7 ) );
        setup_tree_and_handshake( &tree );

        CHECK_FALSE( cli.query_pending() );

        auto cb = [this]( asrtr_param_client*, asrtr_param_query*, uint32_t v ) {
                cb_count++;
                u32_val = v;
        };
        CHECK_EQ( ASRTL_SUCCESS, cli.query< uint32_t >( &query, 2u, cb ) );
        CHECK( cli.query_pending() );

        spin_query();
        CHECK_FALSE( cli.query_pending() );
        CHECK_EQ( 1, cb_count );
        CHECK_EQ( 7u, u32_val );
        asrtl_flat_tree_deinit( &tree );
}

// ============================================================================
// C++ query timeout tests
// ============================================================================

static asrtl_status send_ready_to_client( asrtr::param_client& cli, asrtl_flat_id root_id )
{
        uint8_t  buf[8];
        uint8_t* p = buf;
        *p++       = ASRTL_PARAM_MSG_READY;
        asrtl_add_u32( &p, root_id );
        auto* n = cli.node();
        return n->recv_cb( n->recv_ptr, asrtl_span{ .b = buf, .e = p } );
}

TEST_CASE( "param_client_cpp_timeout" )
{
        asrtl_node head = {};
        head.chid       = ASRTL_CORE;
        collector    coll;
        asrtl_sender sendr = {};
        setup_sender_collector( &sendr, &coll );

        static constexpr uint32_t BUF_SZ      = 256;
        static constexpr uint32_t TIMEOUT     = 10;
        uint8_t                   buf[BUF_SZ] = {};
        asrtr::param_client cli{ &head, sendr, asrtl_span{ .b = buf, .e = buf + BUF_SZ }, TIMEOUT };

        // make ready
        REQUIRE_EQ( ASRTL_SUCCESS, send_ready_to_client( cli, 1u ) );
        REQUIRE_EQ( ASRTL_SUCCESS, cli.tick( 0 ) );
        coll.data.clear();
        REQUIRE( cli.ready() );

        int     called = 0;
        uint8_t err    = 0;
        auto    cb     = [&]( asrtr_param_client*, asrtr_param_query* q, uint32_t ) {
                called++;
                err = q->error_code;
        };
        asrtr_param_query query = {};
        CHECK_EQ( ASRTL_SUCCESS, cli.query< uint32_t >( &query, 10u, cb ) );

        // DELIVER tick → wire
        CHECK_EQ( ASRTL_SUCCESS, cli.tick( 100 ) );
        CHECK_EQ( 0, called );

        // start timing
        CHECK_EQ( ASRTL_SUCCESS, cli.tick( 100 ) );
        CHECK_EQ( 0, called );

        // just before timeout
        CHECK_EQ( ASRTL_SUCCESS, cli.tick( 109 ) );
        CHECK_EQ( 0, called );

        // at timeout
        CHECK_EQ( ASRTL_SUCCESS, cli.tick( 110 ) );
        CHECK_EQ( 1, called );
        CHECK_EQ( ASRTL_PARAM_ERR_TIMEOUT, err );
        CHECK_FALSE( cli.query_pending() );
}

// ============================================================================
// C++ find-by-key tests
// ============================================================================

TEST_CASE_FIXTURE( param_loopback_cpp_ctx, "param_cpp_find_by_key_raw" )
{
        struct asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "a", asrtl_flat_value_u32( 10 ) );
        asrtl_flat_tree_append( &tree, 1, 3, "b", asrtl_flat_value_str( "hi" ) );
        srv.set_tree( &tree );

        auto noop = [] {};
        CHECK_EQ( ASRTL_SUCCESS, srv.send_ready( 1u, noop, 1000 ) );
        spin();
        REQUIRE( cli.ready() );

        // Find "b" by key using raw callback
        CHECK_EQ( ASRTL_SUCCESS, cli.find( &query, 1u, "b", query_cb, this ) );
        spin_query();
        REQUIRE_EQ( 1u, received.size() );
        CHECK_EQ( 3u, received[0].id );
        CHECK_EQ( "b", received[0].key );
        CHECK_EQ( (uint8_t) ASRTL_FLAT_VALUE_TYPE_STR, (uint8_t) received[0].value.type );
        CHECK_EQ( 0, error_called );

        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( typed_loopback_ctx, "typed_find_u32_happy" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "val", asrtl_flat_value_u32( 42 ) );
        asrtl_flat_tree_append( &tree, 1, 3, "other", asrtl_flat_value_str( "x" ) );
        setup_tree_and_handshake( &tree );

        auto cb = [this]( asrtr_param_client*, asrtr_param_query*, uint32_t v ) {
                cb_count++;
                u32_val = v;
        };
        CHECK_EQ( ASRTL_SUCCESS, cli.find< uint32_t >( &query, 1u, "val", cb ) );
        spin_query();
        CHECK_EQ( 1, cb_count );
        CHECK_EQ( 42u, u32_val );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( typed_loopback_ctx, "typed_find_str_happy" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "val", asrtl_flat_value_str( "world" ) );
        setup_tree_and_handshake( &tree );

        auto cb = [this]( asrtr_param_client*, asrtr_param_query*, char const* v ) {
                cb_count++;
                if ( v )
                        str_val = v;
        };
        CHECK_EQ( ASRTL_SUCCESS, cli.find< char const* >( &query, 1u, "val", cb ) );
        spin_query();
        CHECK_EQ( 1, cb_count );
        CHECK_EQ( "world", str_val );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( typed_loopback_ctx, "typed_find_not_found" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "val", asrtl_flat_value_u32( 7 ) );
        setup_tree_and_handshake( &tree );

        auto cb = [this]( asrtr_param_client*, asrtr_param_query* q, uint32_t ) {
                cb_count++;
                got_null = ( q->error_code != 0 );
        };
        CHECK_EQ( ASRTL_SUCCESS, cli.find< uint32_t >( &query, 1u, "missing", cb ) );
        spin_query();
        CHECK_EQ( 1, cb_count );
        CHECK( got_null );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( typed_loopback_ctx, "typed_find_c_callback" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "val", asrtl_flat_value_u32( 55 ) );
        setup_tree_and_handshake( &tree );

        auto c_cb = []( asrtr_param_client*, asrtr_param_query* q, uint32_t v ) {
                auto* ctx = (typed_loopback_ctx*) q->cb_ptr;
                ctx->cb_count++;
                ctx->u32_val = v;
        };
        CHECK_EQ( ASRTL_SUCCESS, cli.find< uint32_t >( &query, 1u, "val", c_cb, this ) );
        spin_query();
        CHECK_EQ( 1, cb_count );
        CHECK_EQ( 55u, u32_val );
        asrtl_flat_tree_deinit( &tree );
}

// ---------------------------------------------------------------------------
// task_unit<T> — coroutine-based test harness

#include "../asrtrpp/task_unit.hpp"

// --- test task definitions ---

struct tu_pass : asrtr::task_test
{
        char const*         name = "tu_pass";
        asrtr::task< void > exec()
        {
                co_return;
        }
};

struct tu_fail : asrtr::task_test
{
        char const*         name = "tu_fail";
        asrtr::task< void > exec()
        {
                co_yield asrtr::with_error{ asrtr::test_fail };
        }
};

struct tu_error : asrtr::task_test
{
        char const*         name = "tu_error";
        asrtr::task< void > exec()
        {
                co_yield asrtr::with_error{ asrtr::test_error };
        }
};

struct tu_multi_pass : asrtr::task_test
{
        char const*         name  = "tu_multi_pass";
        int                 ticks = 0;
        asrtr::task< void > exec()
        {
                for ( int i = 0; i < 4; ++i ) {
                        ++ticks;
                        co_await asrtr::suspend;
                }
        }
};

struct tu_multi_fail : asrtr::task_test
{
        char const*         name = "tu_multi_fail";
        asrtr::task< void > exec()
        {
                co_await asrtr::suspend;
                co_await asrtr::suspend;
                co_yield asrtr::with_error{ asrtr::test_fail };
        }
};

struct tu_multi_error : asrtr::task_test
{
        char const*         name = "tu_multi_error";
        asrtr::task< void > exec()
        {
                co_await asrtr::suspend;
                co_yield asrtr::with_error{ asrtr::test_error };
        }
};

// --- task_unit: construction ---

TEST_CASE( "task_unit_name" )
{
        asrtl::malloc_free_memory_resource mem;
        asrtr::task_ctx                    ctx{ mem };
        asrtr::task_unit< tu_pass >        u{ tu_pass{ ctx } };

        CHECK_EQ( std::string( "tu_pass" ), u.desc );
}

// --- task_unit: direct cb tests ---

struct tu_cb_ctx
{
        asrtl::malloc_free_memory_resource mem;
        asrtr::task_ctx                    ctx{ mem };

        template < typename T >
        void run_to_completion( asrtr::task_unit< T >& u, asrtr_record& rec, int max_ticks = 100 )
        {
                for ( int i = 0; i < max_ticks; ++i ) {
                        asrtr::task_unit< T >::cb( &rec );
                        ctx.tick();
                        if ( rec.state != ASRTR_TEST_RUNNING && rec.state != ASRTR_TEST_INIT )
                                return;
                }
        }
};

TEST_CASE_FIXTURE( tu_cb_ctx, "task_unit_cb_pass" )
{
        asrtr::task_unit< tu_pass > u{ tu_pass{ ctx } };

        asrtr_test_input input{};
        input.test_ptr   = &u;
        input.continue_f = asrtr::task_unit< tu_pass >::cb;

        asrtr_record rec{};
        rec.state = ASRTR_TEST_INIT;
        rec.inpt  = &input;

        run_to_completion( u, rec );
        CHECK_EQ( ASRTR_TEST_PASS, rec.state );
}

TEST_CASE_FIXTURE( tu_cb_ctx, "task_unit_cb_fail" )
{
        asrtr::task_unit< tu_fail > u{ tu_fail{ ctx } };

        asrtr_test_input input{};
        input.test_ptr   = &u;
        input.continue_f = asrtr::task_unit< tu_fail >::cb;

        asrtr_record rec{};
        rec.state = ASRTR_TEST_INIT;
        rec.inpt  = &input;

        run_to_completion( u, rec );
        CHECK_EQ( ASRTR_TEST_FAIL, rec.state );
}

TEST_CASE_FIXTURE( tu_cb_ctx, "task_unit_cb_error" )
{
        asrtr::task_unit< tu_error > u{ tu_error{ ctx } };

        asrtr_test_input input{};
        input.test_ptr   = &u;
        input.continue_f = asrtr::task_unit< tu_error >::cb;

        asrtr_record rec{};
        rec.state = ASRTR_TEST_INIT;
        rec.inpt  = &input;

        run_to_completion( u, rec );
        CHECK_EQ( ASRTR_TEST_ERROR, rec.state );
}

TEST_CASE_FIXTURE( tu_cb_ctx, "task_unit_cb_multi_step_pass" )
{
        asrtr::task_unit< tu_multi_pass > u{ tu_multi_pass{ ctx } };

        asrtr_test_input input{};
        input.test_ptr   = &u;
        input.continue_f = asrtr::task_unit< tu_multi_pass >::cb;

        asrtr_record rec{};
        rec.state = ASRTR_TEST_INIT;
        rec.inpt  = &input;

        // First cb call: sets RUNNING, starts coroutine
        asrtr::task_unit< tu_multi_pass >::cb( &rec );
        CHECK_EQ( ASRTR_TEST_RUNNING, rec.state );

        // Subsequent cb calls are no-ops (state already RUNNING)
        asrtr::task_unit< tu_multi_pass >::cb( &rec );
        CHECK_EQ( ASRTR_TEST_RUNNING, rec.state );

        // Tick the task core until the coroutine completes
        for ( int i = 0; i < 20; ++i ) {
                ctx.tick();
                if ( rec.state != ASRTR_TEST_RUNNING )
                        break;
        }
        CHECK_EQ( ASRTR_TEST_PASS, rec.state );
}

TEST_CASE_FIXTURE( tu_cb_ctx, "task_unit_cb_multi_step_fail" )
{
        asrtr::task_unit< tu_multi_fail > u{ tu_multi_fail{ ctx } };

        asrtr_test_input input{};
        input.test_ptr   = &u;
        input.continue_f = asrtr::task_unit< tu_multi_fail >::cb;

        asrtr_record rec{};
        rec.state = ASRTR_TEST_INIT;
        rec.inpt  = &input;

        run_to_completion( u, rec );
        CHECK_EQ( ASRTR_TEST_FAIL, rec.state );
}

TEST_CASE_FIXTURE( tu_cb_ctx, "task_unit_cb_multi_step_error" )
{
        asrtr::task_unit< tu_multi_error > u{ tu_multi_error{ ctx } };

        asrtr_test_input input{};
        input.test_ptr   = &u;
        input.continue_f = asrtr::task_unit< tu_multi_error >::cb;

        asrtr_record rec{};
        rec.state = ASRTR_TEST_INIT;
        rec.inpt  = &input;

        run_to_completion( u, rec );
        CHECK_EQ( ASRTR_TEST_ERROR, rec.state );
}

// cb is no-op when state is not INIT (coroutine already started)
TEST_CASE_FIXTURE( tu_cb_ctx, "task_unit_cb_noop_after_start" )
{
        asrtr::task_unit< tu_multi_pass > u{ tu_multi_pass{ ctx } };

        asrtr_test_input input{};
        input.test_ptr   = &u;
        input.continue_f = asrtr::task_unit< tu_multi_pass >::cb;

        asrtr_record rec{};
        rec.state = ASRTR_TEST_INIT;
        rec.inpt  = &input;

        // Start the coroutine
        auto st = asrtr::task_unit< tu_multi_pass >::cb( &rec );
        CHECK_EQ( ASRTR_SUCCESS, st );
        CHECK_EQ( ASRTR_TEST_RUNNING, rec.state );

        // Calling cb again should not restart — still RUNNING
        st = asrtr::task_unit< tu_multi_pass >::cb( &rec );
        CHECK_EQ( ASRTR_SUCCESS, st );
        CHECK_EQ( ASRTR_TEST_RUNNING, rec.state );
}

// --- task_unit: reactor integration ---

struct tu_e2e_ctx
{
        asrtl::malloc_free_memory_resource mem;
        asrtr::task_ctx                    ctx{ mem };
        collector                          coll;
        collect_sender                     send_fn{ &coll };
        asrtr::reactor                     r{ send_fn, "task_reactor" };

        std::shared_ptr< asrtr::task_unit< tu_pass > >       t0;
        std::shared_ptr< asrtr::task_unit< tu_fail > >       t1;
        std::shared_ptr< asrtr::task_unit< tu_error > >      t2;
        std::shared_ptr< asrtr::task_unit< tu_multi_pass > > t3;

        tu_e2e_ctx()
          : t0( std::make_shared< asrtr::task_unit< tu_pass > >( tu_pass{ ctx } ) )
          , t1( std::make_shared< asrtr::task_unit< tu_fail > >( tu_fail{ ctx } ) )
          , t2( std::make_shared< asrtr::task_unit< tu_error > >( tu_error{ ctx } ) )
          , t3( std::make_shared< asrtr::task_unit< tu_multi_pass > >( tu_multi_pass{ ctx } ) )
        {
                r.add_test( *t0 );
                r.add_test( *t1 );
                r.add_test( *t2 );
                r.add_test( *t3 );
        }

        ~tu_e2e_ctx()
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
                for ( int i = 0; i < 50; i++ ) {
                        ctx.tick();
                        CHECK_EQ( ASRTR_SUCCESS, r.tick() );
                }
        }
};

TEST_CASE_FIXTURE( tu_e2e_ctx, "task_unit_e2e_pass" )
{
        run( 0, 100 );
        REQUIRE_EQ( 2u, coll.data.size() );
        assert_test_result_msg( coll.data.back(), 100, ASRTL_TEST_SUCCESS );
        coll.data.pop_back();
        assert_test_start_msg( coll.data.back(), 0, 100 );
        coll.data.pop_back();
}

TEST_CASE_FIXTURE( tu_e2e_ctx, "task_unit_e2e_fail" )
{
        run( 1, 200 );
        REQUIRE_EQ( 2u, coll.data.size() );
        assert_test_result_msg( coll.data.back(), 200, ASRTL_TEST_FAILURE );
        coll.data.pop_back();
        assert_test_start_msg( coll.data.back(), 1, 200 );
        coll.data.pop_back();
}

TEST_CASE_FIXTURE( tu_e2e_ctx, "task_unit_e2e_error" )
{
        run( 2, 300 );
        REQUIRE_EQ( 2u, coll.data.size() );
        assert_test_result_msg( coll.data.back(), 300, ASRTL_TEST_ERROR );
        coll.data.pop_back();
        assert_test_start_msg( coll.data.back(), 2, 300 );
        coll.data.pop_back();
}

TEST_CASE_FIXTURE( tu_e2e_ctx, "task_unit_e2e_multi_step" )
{
        run( 3, 400 );
        REQUIRE_EQ( 2u, coll.data.size() );
        assert_test_result_msg( coll.data.back(), 400, ASRTL_TEST_SUCCESS );
        coll.data.pop_back();
        assert_test_start_msg( coll.data.back(), 3, 400 );
        coll.data.pop_back();
}

// ---------------------------------------------------------------------------
// param_query_sender — param() and find() free functions

#include "../asrtrpp/param_sender.hpp"

// Test fixture: cross-wired param loopback with task_ctx for coroutine tests.
// The sender under test (param_query_sender) is async — the callback fires
// during cli.tick() when the server response arrives.  For coroutine tests
// we also need to tick the ecor task_core so that co_await resumes.
struct param_sender_ctx
{
        asrtl_node srv_head = {};
        asrtl_node cli_head = {};

        std::function< asrtl_status( asrtl_chann_id, asrtl_rec_span* ) > srv_send{
            [this]( asrtl_chann_id, asrtl_rec_span* buff ) {
                    auto flat = flatten( buff );
                    auto sp   = asrtl::cnv( std::span{ flat } );
                    cli.node()->recv_cb( cli.node()->recv_ptr, sp );
                    return ASRTL_SUCCESS;
            } };

        std::function< asrtl_status( asrtl_chann_id, asrtl_rec_span* ) > cli_send{
            [this]( asrtl_chann_id, asrtl_rec_span* buff ) {
                    auto flat = flatten( buff );
                    auto sp   = asrtl::cnv( std::span{ flat } );
                    srv.node()->recv_cb( srv.node()->recv_ptr, sp );
                    return ASRTL_SUCCESS;
            } };

        asrtc::param_server srv{ &srv_head, srv_send, asrtl_default_allocator() };

        static constexpr uint32_t BUF_SZ          = 256;
        uint8_t                   cli_buf[BUF_SZ] = {};
        asrtr::param_client       cli{
            &cli_head,
            cli_send,
            asrtl_span{ .b = cli_buf, .e = cli_buf + BUF_SZ },
            100 };

        asrtl::malloc_free_memory_resource mem;
        asrtr::task_ctx                    tctx{ mem };
        uint32_t                           t = 1;

        param_sender_ctx()
        {
                srv_head.chid = ASRTL_CORE;
                cli_head.chid = ASRTL_CORE;
        }

        void setup_tree_and_handshake( asrtl_flat_tree* tree )
        {
                srv.set_tree( tree );
                auto noop = [] {};
                CHECK_EQ( ASRTL_SUCCESS, srv.send_ready( 1u, noop, 1000 ) );
                for ( int i = 0; i < 100; i++ ) {
                        srv.tick( t++ );
                        cli.tick();
                        if ( cli.ready() )
                                break;
                }
                REQUIRE( cli.ready() );
        }

        void spin( int max_iter = 200 )
        {
                for ( int i = 0; i < max_iter; i++ ) {
                        srv.tick( t++ );
                        cli.tick();
                        tctx.tick();
                }
        }

        // Run a task<void> to completion, return the final test state.
        // F should be a callable that takes (task_ctx&) and returns task<void>.
        template < typename F >
        asrtr_test_state run_task( F&& make_task )
        {
                struct test_recv
                {
                        using receiver_concept = ecor::receiver_t;
                        asrtr_test_state* out;

                        void set_value()
                        {
                                *out = ASRTR_TEST_PASS;
                        }
                        void set_error( asrtr::task_error e )
                        {
                                *out =
                                    ( e == asrtr::test_error ) ? ASRTR_TEST_ERROR : ASRTR_TEST_FAIL;
                        }
                        void set_error( ecor::task_error )
                        {
                                *out = ASRTR_TEST_FAIL;
                        }
                        void set_stopped()
                        {
                                *out = ASRTR_TEST_FAIL;
                        }
                };
                asrtr_test_state result = ASRTR_TEST_INIT;
                auto             op     = make_task( tctx ).connect( test_recv{ &result } );
                op.start();
                for ( int i = 0; i < 200 && result == ASRTR_TEST_INIT; i++ ) {
                        srv.tick( t++ );
                        cli.tick();
                        tctx.tick();
                }
                return result;
        }
};

// Free-function coroutines (ecor requires task_ctx& as first arg, no lambda coroutines).

template < typename T >
asrtr::task< void > ps_do_param( asrtr::task_ctx&, asrtr::param_client& c, uint16_t id )
{
        co_await asrtr::param< T >( c, id );
}

template < typename T >
asrtr::task< void > ps_do_find(
    asrtr::task_ctx&,
    asrtr::param_client& c,
    uint16_t             parent,
    char const*          key )
{
        co_await asrtr::find< T >( c, parent, key );
}

asrtr::task< void > ps_capture_find_u32(
    asrtr::task_ctx&,
    asrtr::param_client& c,
    uint16_t             parent,
    char const*          key,
    uint32_t*            out )
{
        *out = co_await asrtr::find< uint32_t >( c, parent, key );
}

asrtr::task< void > ps_capture_find_str(
    asrtr::task_ctx&,
    asrtr::param_client& c,
    uint16_t             parent,
    char const*          key,
    std::string*         out )
{
        auto v = co_await asrtr::find< char const* >( c, parent, key );
        *out   = v;
}

asrtr::task< void > ps_find_u32_then_check(
    asrtr::task_ctx&,
    asrtr::param_client& c,
    uint16_t             parent,
    char const*          key,
    bool*                reached )
{
        auto v = co_await asrtr::find< uint32_t >( c, parent, key );
        (void) v;
        *reached = true;
}

asrtr::task< void > ps_sequential_finds(
    asrtr::task_ctx&,
    asrtr::param_client& c,
    uint16_t             parent,
    char const*          key_a,
    char const*          key_b,
    uint32_t*            sum )
{
        auto a = co_await asrtr::find< uint32_t >( c, parent, key_a );
        auto b = co_await asrtr::find< uint32_t >( c, parent, key_b );
        *sum   = a + b;
}

asrtr::task< void > ps_second_fails(
    asrtr::task_ctx&,
    asrtr::param_client& c,
    uint16_t             parent,
    char const*          key_a,
    char const*          key_b,
    uint32_t*            first_val,
    bool*                reached )
{
        *first_val = co_await asrtr::find< uint32_t >( c, parent, key_a );
        auto b     = co_await asrtr::find< uint32_t >( c, parent, key_b );
        (void) b;
        *reached = true;
}

// --- param<T>: happy paths ---

TEST_CASE_FIXTURE( param_sender_ctx, "ps_param_u32_happy" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_u32( 42 ) );
        setup_tree_and_handshake( &tree );

        auto state = run_task( [&]( asrtr::task_ctx& ctx ) {
                return ps_do_param< uint32_t >( ctx, cli, 1 );
        } );
        CHECK_EQ( ASRTR_TEST_PASS, state );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_sender_ctx, "ps_param_i32_happy" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_i32( -5 ) );
        setup_tree_and_handshake( &tree );

        auto state = run_task( [&]( asrtr::task_ctx& ctx ) {
                return ps_do_param< int32_t >( ctx, cli, 1 );
        } );
        CHECK_EQ( ASRTR_TEST_PASS, state );
        asrtl_flat_tree_deinit( &tree );
}

// --- find<T>: happy paths ---

TEST_CASE_FIXTURE( param_sender_ctx, "ps_find_u32_happy" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "count", asrtl_flat_value_u32( 7 ) );
        setup_tree_and_handshake( &tree );

        auto state = run_task( [&]( asrtr::task_ctx& ctx ) {
                return ps_do_find< uint32_t >( ctx, cli, 1, "count" );
        } );
        CHECK_EQ( ASRTR_TEST_PASS, state );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_sender_ctx, "ps_find_str_happy" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "msg", asrtl_flat_value_str( "hello" ) );
        setup_tree_and_handshake( &tree );

        auto state = run_task( [&]( asrtr::task_ctx& ctx ) {
                return ps_do_find< char const* >( ctx, cli, 1, "msg" );
        } );
        CHECK_EQ( ASRTR_TEST_PASS, state );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_sender_ctx, "ps_find_float_happy" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "pi", asrtl_flat_value_float( 3.14f ) );
        setup_tree_and_handshake( &tree );

        auto state = run_task( [&]( asrtr::task_ctx& ctx ) {
                return ps_do_find< float >( ctx, cli, 1, "pi" );
        } );
        CHECK_EQ( ASRTR_TEST_PASS, state );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_sender_ctx, "ps_find_obj_happy" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "sub", asrtl_flat_value_object() );
        setup_tree_and_handshake( &tree );

        auto state = run_task( [&]( asrtr::task_ctx& ctx ) {
                return ps_do_find< asrtr::param_obj >( ctx, cli, 1, "sub" );
        } );
        CHECK_EQ( ASRTR_TEST_PASS, state );
        asrtl_flat_tree_deinit( &tree );
}

// --- error: type mismatch (callback fires with error_code) ---

TEST_CASE_FIXTURE( param_sender_ctx, "ps_param_type_mismatch" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_str( "nope" ) );
        setup_tree_and_handshake( &tree );

        // Request u32 but node is a string → TYPE_MISMATCH → set_error
        auto state = run_task( [&]( asrtr::task_ctx& ctx ) {
                return ps_do_param< uint32_t >( ctx, cli, 1 );
        } );
        CHECK_EQ( ASRTR_TEST_FAIL, state );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_sender_ctx, "ps_find_type_mismatch" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "val", asrtl_flat_value_str( "nope" ) );
        setup_tree_and_handshake( &tree );

        // Request u32 but "val" is a string → TYPE_MISMATCH
        auto state = run_task( [&]( asrtr::task_ctx& ctx ) {
                return ps_do_find< uint32_t >( ctx, cli, 1, "val" );
        } );
        CHECK_EQ( ASRTR_TEST_FAIL, state );
        asrtl_flat_tree_deinit( &tree );
}

// --- error: key not found ---

TEST_CASE_FIXTURE( param_sender_ctx, "ps_find_key_not_found" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "val", asrtl_flat_value_u32( 5 ) );
        setup_tree_and_handshake( &tree );

        auto state = run_task( [&]( asrtr::task_ctx& ctx ) {
                return ps_do_find< uint32_t >( ctx, cli, 1, "missing" );
        } );
        CHECK_EQ( ASRTR_TEST_FAIL, state );
        asrtl_flat_tree_deinit( &tree );
}

// --- error: client not ready (find returns ASRTL_ARG_ERR → immediate set_error) ---

TEST_CASE_FIXTURE( param_sender_ctx, "ps_param_not_ready" )
{
        // Don't handshake — client is not ready
        auto state = run_task( [&]( asrtr::task_ctx& ctx ) {
                return ps_do_param< uint32_t >( ctx, cli, 1 );
        } );
        CHECK_EQ( ASRTR_TEST_FAIL, state );
}

TEST_CASE_FIXTURE( param_sender_ctx, "ps_find_not_ready" )
{
        auto state = run_task( [&]( asrtr::task_ctx& ctx ) {
                return ps_do_find< uint32_t >( ctx, cli, 1, "x" );
        } );
        CHECK_EQ( ASRTR_TEST_FAIL, state );
}

// --- error: query already pending (second sender fails immediately) ---

TEST_CASE_FIXTURE( param_sender_ctx, "ps_param_query_pending" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "a", asrtl_flat_value_u32( 1 ) );
        asrtl_flat_tree_append( &tree, 1, 3, "b", asrtl_flat_value_u32( 2 ) );
        setup_tree_and_handshake( &tree );

        // Start a raw query to occupy the pending slot
        asrtr_param_query q  = {};
        auto              cb = []( asrtr_param_client*, asrtr_param_query*, uint32_t ) {};
        CHECK_EQ( ASRTL_SUCCESS, cli.query< uint32_t >( &q, 2u, cb, nullptr ) );
        CHECK( cli.query_pending() );

        // Now a param sender should fail because a query is already pending
        auto state = run_task( [&]( asrtr::task_ctx& ctx ) {
                return ps_do_param< uint32_t >( ctx, cli, 3 );
        } );
        CHECK_EQ( ASRTR_TEST_FAIL, state );

        // Drain the pending query
        spin();
        asrtl_flat_tree_deinit( &tree );
}

// --- coroutine integration: value returned via co_await ---

TEST_CASE_FIXTURE( param_sender_ctx, "ps_param_value_in_coroutine" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "count", asrtl_flat_value_u32( 42 ) );
        setup_tree_and_handshake( &tree );

        uint32_t captured = 0;
        auto     state    = run_task( [&]( asrtr::task_ctx& ctx ) {
                return ps_capture_find_u32( ctx, cli, 1, "count", &captured );
        } );
        CHECK_EQ( ASRTR_TEST_PASS, state );
        CHECK_EQ( 42u, captured );
        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_sender_ctx, "ps_find_value_in_coroutine" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "msg", asrtl_flat_value_str( "hi" ) );
        setup_tree_and_handshake( &tree );

        std::string captured;
        auto        state = run_task( [&]( asrtr::task_ctx& ctx ) {
                return ps_capture_find_str( ctx, cli, 1, "msg", &captured );
        } );
        CHECK_EQ( ASRTR_TEST_PASS, state );
        CHECK_EQ( "hi", captured );
        asrtl_flat_tree_deinit( &tree );
}

// --- coroutine integration: error propagates and aborts the task ---

TEST_CASE_FIXTURE( param_sender_ctx, "ps_error_propagates_in_coroutine" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "val", asrtl_flat_value_str( "nope" ) );
        setup_tree_and_handshake( &tree );

        bool reached = false;
        auto state   = run_task( [&]( asrtr::task_ctx& ctx ) {
                return ps_find_u32_then_check( ctx, cli, 1, "val", &reached );
        } );
        CHECK_EQ( ASRTR_TEST_FAIL, state );
        CHECK_FALSE( reached );
        asrtl_flat_tree_deinit( &tree );
}

// --- coroutine integration: sequential co_awaits ---

TEST_CASE_FIXTURE( param_sender_ctx, "ps_sequential_queries_in_coroutine" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "a", asrtl_flat_value_u32( 10 ) );
        asrtl_flat_tree_append( &tree, 1, 3, "b", asrtl_flat_value_u32( 20 ) );
        setup_tree_and_handshake( &tree );

        uint32_t sum   = 0;
        auto     state = run_task( [&]( asrtr::task_ctx& ctx ) {
                return ps_sequential_finds( ctx, cli, 1, "a", "b", &sum );
        } );
        CHECK_EQ( ASRTR_TEST_PASS, state );
        CHECK_EQ( 30u, sum );
        asrtl_flat_tree_deinit( &tree );
}

// --- coroutine integration: second query fails, first succeeded ---

TEST_CASE_FIXTURE( param_sender_ctx, "ps_second_query_fails_in_coroutine" )
{
        asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append( &tree, 0, 1, nullptr, asrtl_flat_value_object() );
        asrtl_flat_tree_append( &tree, 1, 2, "a", asrtl_flat_value_u32( 10 ) );
        setup_tree_and_handshake( &tree );

        uint32_t first_val = 0;
        bool     reached   = false;
        auto     state     = run_task( [&]( asrtr::task_ctx& ctx ) {
                return ps_second_fails( ctx, cli, 1, "a", "missing", &first_val, &reached );
        } );
        CHECK_EQ( ASRTR_TEST_FAIL, state );
        CHECK_EQ( 10u, first_val );
        CHECK_FALSE( reached );
        asrtl_flat_tree_deinit( &tree );
}
