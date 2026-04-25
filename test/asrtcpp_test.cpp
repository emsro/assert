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

#include "../asrtc/assembly.h"
#include "../asrtc/result.h"
#include "../asrtcpp/collect.hpp"
#include "../asrtcpp/controller.hpp"
#include "../asrtcpp/diag.hpp"
#include "../asrtcpp/param.hpp"
#include "../asrtl/collect_proto.h"
#include "../asrtl/log.h"
#include "../asrtlpp/callback.hpp"
#include "../asrtlpp/fmt.hpp"
#include "../asrtlpp/util.hpp"
#include "../asrtrpp/diag.hpp"
#include "../asrtrpp/reactor.hpp"
#include "./collector.hpp"
#include "./util.h"

#include <doctest/doctest.h>
#include <format>
#include <string>
#include <vector>

ASRTL_DEFINE_GPOS_LOG()

// ---------------------------------------------------------------------------
// helpers

static std::vector< uint8_t > flatten( asrt::rec_span const* buff )
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
        char const*  name() const { return "passing_test"; }
        asrtl_status operator()( asrt::record& rec )
        {
                rec.state = ASRTR_TEST_PASS;
                return ASRTL_SUCCESS;
        }
};

struct failing_test
{
        char const*  name() const { return "failing_test"; }
        asrtl_status operator()( asrt::record& rec )
        {
                rec.state = ASRTR_TEST_FAIL;
                return ASRTL_SUCCESS;
        }
};

// ---------------------------------------------------------------------------
// paired fixture: controller <-> reactor wired in-process
//
// Member declaration order governs construction:
//   counters / status first, then the optional controller (starts empty),
//   then the reactor unit stubs, then the reactor itself.
// The constructor body emplaces the controller and completes the handshake.

struct paired_ctx
{
        int          init_cb_count = 0;
        asrt::status init_status   = ASRTL_SUCCESS;
        uint32_t     t             = 1;


        asrt::unit< passing_test > t0;
        asrt::unit< failing_test > t1;

        // Must be declared before `r` so it is constructed (and valid) before the reactor
        // captures &r_send as its sender pointer.  asrtlpp/sender.hpp stores &CB, so the
        // lambda object must outlive the reactor.
        std::function< asrt::status( asrtl_chann_id, asrt::rec_span*, asrtl_send_done_cb, void* ) >
            r_send;

        // Stable send callable for the controller; must outlive `c`.
        std::function< asrt::status( asrtl_chann_id, asrt::rec_span*, asrtl_send_done_cb, void* ) >
            c_send{ [this](
                        asrtl_chann_id,
                        asrtl_rec_span*    buff,
                        asrtl_send_done_cb done_cb,
                        void*              done_ptr ) {
                    auto              flat = flatten( buff );
                    auto              sp   = asrt::cnv( std::span{ flat } );
                    auto*             rn   = &r.node;
                    enum asrtl_status st   = asrtl_chann_recv( rn, sp );
                    if ( done_cb )
                            done_cb( done_ptr, st );
                    return st;
            } };

        asrtc_controller c;
        asrtr_reactor    r;

        paired_ctx()
          : r_send( [this](
                        asrtl_chann_id,
                        asrtl_rec_span*    buff,
                        asrtl_send_done_cb done_cb,
                        void*              done_ptr ) {
                  auto              flat = flatten( buff );
                  auto              sp   = asrt::cnv( std::span{ flat } );
                  auto*             cn   = &c.node;
                  enum asrtl_status st   = asrtl_chann_recv( cn, sp );
                  if ( done_cb )
                          done_cb( done_ptr, st );
                  return st;
          } )
        {
                if ( asrt::init( &r, asrt::autosender( r_send ), "paired_reactor" ) !=
                     ASRTL_SUCCESS )
                        throw std::runtime_error( "reactor init failed" );

                (void) asrt::add_test( r, t0 );
                (void) asrt::add_test( r, t1 );

                if ( asrt::init(
                         c,
                         c_send,
                         asrtl_default_allocator() ) != ASRTL_SUCCESS )
                        throw std::runtime_error( "controller init failed" );

                // start init handshake
                (void) asrt::start(
                    c,
                    { []( void* self, asrt::status s ) -> asrt::status {
                             auto* p = static_cast< paired_ctx* >( self );
                             p->init_cb_count++;
                             p->init_status = s;
                             return ASRTL_SUCCESS;
                     },
                      this },
                    1000 );

                // complete PROTO_VERSION handshake
                for ( int i = 0; i < 100 && !asrt::is_idle( c ); i++ ) {
                        (void) asrt::tick( asrt::node( c ), t++ );
                        (void) asrt::tick( asrt::node( r ), t++ );
                }
                CHECK( asrt::is_idle( c ) );
        }

        ~paired_ctx()
        {
                asrt::deinit( c );
                asrt::deinit( r );
        }

        // tick both sides until controller is idle again
        void spin()
        {
                for ( int i = 0; i < 100 && !asrt::is_idle( c ); i++ ) {
                        (void) asrt::tick( asrt::node( c ), t++ );
                        (void) asrt::tick( asrt::node( r ), t++ );
                }
                CHECK( asrt::is_idle( c ) );
        }
};

// ---------------------------------------------------------------------------
// fmt

TEST_CASE( "fmt_success" )
{
        std::string s = std::format( "{}", ASRTL_SUCCESS );
        CHECK_EQ( s, asrtl_status_to_str( ASRTL_SUCCESS ) );
}

TEST_CASE( "fmt_error" )
{
        std::string s = std::format( "{}", ASRTL_INIT_ERR );
        CHECK_EQ( s, asrtl_status_to_str( ASRTL_INIT_ERR ) );
}

// ---------------------------------------------------------------------------
// controller init

TEST_CASE_FIXTURE( paired_ctx, "controller_init" )
{
        CHECK( asrt::is_idle( c ) );
        CHECK_EQ( ASRTL_CORE, asrt::node( c ).chid );
        CHECK_EQ( ASRTL_SUCCESS, init_status );
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
        std::string  received;
        asrt::status st = asrt::query_desc(
            c,
            { []( void* self, asrt::status s, char* desc ) -> asrt::status {
                     auto* p = static_cast< std::string* >( self );
                     CHECK_EQ( ASRTL_SUCCESS, s );
                     *p = std::string{ desc };
                     return ASRTL_SUCCESS;
             },
              &received },
            1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
        spin();
        CHECK_EQ( "paired_reactor", received );
}

// ---------------------------------------------------------------------------
// query_test_count

TEST_CASE_FIXTURE( paired_ctx, "query_test_count" )
{
        // C callback uses uint16_t; store widened value in uint32_t for assertions.
        uint32_t     count = 0;
        asrt::status st    = asrt::query_test_count(
            c,
            { []( void* p, asrt::status s, uint16_t n ) -> asrt::status {
                     CHECK_EQ( ASRTL_SUCCESS, s );
                     auto* count_ptr = static_cast< uint32_t* >( p );
                     *count_ptr      = n;
                     return ASRTL_SUCCESS;
             },
                 &count },
            1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
        spin();
        CHECK_EQ( 2u, count );
}

// ---------------------------------------------------------------------------
// query_test_info

TEST_CASE_FIXTURE( paired_ctx, "query_test_info" )
{
        uint16_t    tid = 0xFFFF;
        std::string name;
        struct ctx
        {
                uint16_t*    tid;
                std::string* name;
        } cb_ctx = { &tid, &name };

        asrt::status st = asrt::query_test_info(
            c,
            0,
            { []( void* p, asrt::status s, uint16_t t, char* desc ) -> asrt::status {
                     auto* ctx = static_cast< struct ctx* >( p );
                     CHECK_EQ( ASRTL_SUCCESS, s );
                     *ctx->tid  = t;
                     *ctx->name = std::string{ desc };
                     return ASRTL_SUCCESS;
             },
              &cb_ctx },
            1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
        spin();
        CHECK_EQ( 0u, tid );
        CHECK_EQ( "passing_test", name );
}

// ---------------------------------------------------------------------------
// exec_test

TEST_CASE_FIXTURE( paired_ctx, "exec_test_pass" )
{
        asrtc_test_result res = ASRTC_TEST_UNKNOWN;
        asrt::status      st  = asrt::exec_test(
            c,
            0,
            { []( void* p, asrt::status s, asrtc_result* r ) -> asrt::status {
                     auto* out = static_cast< asrtc_test_result* >( p );
                     CHECK_EQ( ASRTL_SUCCESS, s );
                     *out = r->res;
                     return ASRTL_SUCCESS;
             },
                    &res },
            1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
        spin();
        CHECK_EQ( ASRTC_TEST_SUCCESS, res );
}

TEST_CASE_FIXTURE( paired_ctx, "exec_test_fail" )
{
        asrtc_test_result res = ASRTC_TEST_UNKNOWN;
        asrt::status      st  = asrt::exec_test(
            c,
            1,
            { []( void* p, asrt::status s, asrtc_result* r ) -> asrt::status {
                     auto* out = static_cast< asrtc_test_result* >( p );
                     CHECK_EQ( ASRTL_SUCCESS, s );
                     *out = r->res;
                     return ASRTL_SUCCESS;
             },
                    &res },
            1000 );
        CHECK_EQ( ASRTL_SUCCESS, st );
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
        CHECK_EQ(
            ASRTL_SUCCESS,
            asrt::query_desc(
                c,
                { []( void* p, asrt::status, char* ) -> asrt::status {
                         auto* called = static_cast< bool* >( p );
                         *called      = true;
                         return ASRTL_SUCCESS;
                 },
                  &first_called },
                1000 ) );

        // controller is now busy — second query must be rejected; its callback must NOT be stored
        CHECK_EQ(
            ASRTL_BUSY_ERR,
            asrt::query_test_count(
                c,
                { []( void* p, asrt::status, uint16_t ) -> asrt::status {
                         auto* called = static_cast< bool* >( p );
                         *called      = true;
                         return ASRTL_SUCCESS;
                 },
                  &second_called },
                1000 ) );

        // complete the first query
        spin();

        CHECK( first_called );
        CHECK_FALSE( second_called );
}

// ---------------------------------------------------------------------------
// controller + diagnostics
//
// Extends paired_ctx with an diag on the reactor side and an
// diag on the controller side, wired bidirectionally.
//
// The in-process paired_ctx routes messages directly via recv_cb, bypassing
// channel dispatch.  Each diag sender delivers straight to the peer's
// recv_cb.  Construction order: send lambdas first (capture this, only called
// at runtime), then the diag objects that consume them.

struct diag_paired_ctx : paired_ctx
{
        // c_diag sends → r_diag (controller-to-reactor direction)
        std::function< asrtl_status( asrtl_chann_id, asrtl_rec_span*, asrtl_send_done_cb, void* ) >
            c_diag_send{ [this](
                             asrtl_chann_id,
                             asrtl_rec_span*    buff,
                             asrtl_send_done_cb done_cb,
                             void*              done_ptr ) {
                    auto              flat = flatten( buff );
                    auto&             rn   = asrt::node( r_diag );
                    enum asrtl_status st   = asrt::recv( rn, flat );
                    if ( done_cb )
                            done_cb( done_ptr, st );
                    return st;
            } };

        asrtc_diag c_diag;

        // r_diag sends → c_diag (reactor-to-controller direction)
        std::function< asrtl_status( asrtl_chann_id, asrtl_rec_span*, asrtl_send_done_cb, void* ) >
            r_diag_send{ [this](
                             asrtl_chann_id,
                             asrtl_rec_span*    buff,
                             asrtl_send_done_cb done_cb,
                             void*              done_ptr ) {
                    auto              flat = flatten( buff );
                    auto&             rn   = asrt::node( c_diag );
                    enum asrtl_status st   = asrt::recv( rn, flat );
                    if ( done_cb )
                            done_cb( done_ptr, st );
                    return st;
            } };

        asrtr_diag r_diag;

        diag_paired_ctx()
        {
                if ( asrt::init(
                         c_diag, asrt::node( this->c ), c_diag_send, asrtl_default_allocator() ) !=
                     ASRTL_SUCCESS )
                        throw std::runtime_error( "r_diag init failed" );
                if ( asrt::init( r_diag, asrt::node( this->r ), r_diag_send ) != ASRTL_SUCCESS )
                        throw std::runtime_error( "c_diag init failed" );
        }

        ~diag_paired_ctx()
        {
                asrt::deinit( r_diag );
                asrt::deinit( c_diag );
        }
};

TEST_CASE_FIXTURE( diag_paired_ctx, "diag_record_received_by_controller" )
{
        asrt::rec_diag( r_diag, "test_file.c", 42 );

        auto rec = asrt::take_record( c_diag );
        REQUIRE( rec != nullptr );
        CHECK_EQ( 42u, rec->line );
        CHECK( std::string_view{ rec->file }.ends_with( "test_file.c" ) );

        // no more records
        CHECK( asrt::take_record( c_diag ) == nullptr );
}

TEST_CASE_FIXTURE( diag_paired_ctx, "diag_multiple_records_queued_in_order" )
{
        asrt::rec_diag( r_diag, "a.c", 1 );
        asrt::rec_diag( r_diag, "b.c", 2 );
        asrt::rec_diag( r_diag, "c.c", 3 );

        auto r1 = asrt::take_record( c_diag );
        auto r2 = asrt::take_record( c_diag );
        auto r3 = asrt::take_record( c_diag );
        REQUIRE( r1 );
        REQUIRE( r2 );
        REQUIRE( r3 );

        CHECK_EQ( 1u, r1->line );
        CHECK_EQ( 2u, r2->line );
        CHECK_EQ( 3u, r3->line );

        CHECK( asrt::take_record( c_diag ) == nullptr );
}

TEST_CASE_FIXTURE( diag_paired_ctx, "diag_independent_of_controller_queries" )
{
        // both a controller query and a diag record in flight at the same time
        std::string desc;
        auto        on_desc = [&]( asrt::status s, char* sv ) -> asrt::status {
                CHECK_EQ( ASRTL_SUCCESS, s );
                desc = std::string{ sv };
                return ASRTL_SUCCESS;
        };
        CHECK_EQ( ASRTL_SUCCESS, asrt::query_desc( c, on_desc, 1000 ) );

        asrt::rec_diag( r_diag, "diag_test.c", 99 );

        spin();

        CHECK_EQ( "paired_reactor", desc );

        auto rec = asrt::take_record( c_diag );
        REQUIRE( rec != nullptr );
        CHECK_EQ( 99u, rec->line );
}

// ---------------------------------------------------------------------------
// param_server wrapper

struct param_server_ctx : paired_ctx
{
        collector param_coll;

        std::function< asrtl_status( asrtl_chann_id, asrtl_rec_span*, asrtl_send_done_cb, void* ) >
            param_send{ [this](
                            asrtl_chann_id     id,
                            asrtl_rec_span*    buff,
                            asrtl_send_done_cb done_cb,
                            void*              done_ptr ) {
                    return sender_collect( &param_coll, id, buff, done_cb, done_ptr );
            } };

        asrtc_param_server srv;

        param_server_ctx()
        {
                if ( asrt::init( &srv, asrt::node( c ), param_send, asrtl_default_allocator() ) !=
                     ASRTL_SUCCESS )
                        throw std::runtime_error( "param_server init failed" );
        }

        ~param_server_ctx() { asrt::deinit( srv ); }
};

TEST_CASE_FIXTURE( param_server_ctx, "param_server_node_chained" )
{
        CHECK_EQ( ASRTL_PARA, asrt::node( srv ).chid );
}

TEST_CASE_FIXTURE( param_server_ctx, "param_server_set_tree_and_send_ready" )
{
        struct asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append_cont( &tree, 0, 1, nullptr, ASRTL_FLAT_CTYPE_OBJECT );
        asrtl_flat_tree_append_scalar( &tree, 1, 2, "k", ASRTL_FLAT_STYPE_U32, { .u32_val = 42 } );

        asrt::set_tree( srv, tree );
        auto noop = []( asrtl_status ) {};
        CHECK_EQ( ASRTL_SUCCESS, asrt::send_ready( srv, 1u, noop, 1000 ) );

        REQUIRE_EQ( 1u, param_coll.data.size() );
        CHECK_EQ( ASRTL_PARA, param_coll.data.front().id );
        CHECK_EQ( ASRTL_PARAM_MSG_READY, param_coll.data.front().data[0] );
        param_coll.data.pop_front();

        asrtl_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_server_ctx, "param_server_ready_ack_cb_fires" )
{
        struct asrtl_flat_tree tree;
        asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 16 );
        asrtl_flat_tree_append_cont( &tree, 0, 1, nullptr, ASRTL_FLAT_CTYPE_OBJECT );

        asrt::set_tree( srv, tree );

        int  ack_count = 0;
        auto on_ack    = [&]( asrtl_status ) {
                ++ack_count;
        };
        CHECK_EQ( ASRTL_SUCCESS, asrt::send_ready( srv, 1u, on_ack, 1000 ) );
        param_coll.data.clear();

        // Build a READY_ACK message: [ASRTL_PARAM_MSG_READY_ACK, max_msg_size(256) LE]
        uint8_t    ack_msg[5] = { ASRTL_PARAM_MSG_READY_ACK, 0, 1, 0, 0 };  // 256 LE
        asrtl_span sp         = { .b = ack_msg, .e = ack_msg + sizeof ack_msg };

        auto& n = asrt::node( srv );
        asrt::recv( n, sp );

        CHECK_EQ( 0, ack_count );  // not yet — pending, needs tick
        CHECK_EQ( ASRTL_SUCCESS, asrt::tick( asrt::node( srv ), t++ ) );
        CHECK_EQ( 1, ack_count );  // fired once

        // Second tick should not fire again
        CHECK_EQ( ASRTL_SUCCESS, asrt::tick( asrt::node( srv ), t++ ) );
        CHECK_EQ( 1, ack_count );

        asrtl_flat_tree_deinit( &tree );
}

// ---------------------------------------------------------------------------
// collect_server wrapper
// ---------------------------------------------------------------------------

static inline asrtl_status inject_csrv_msg( asrtl_node* n, uint8_t* b, uint8_t* e )
{
        return asrtl_chann_recv( n, (asrtl_span) { .b = b, .e = e } );
}

static uint8_t* build_csrv_ready_ack( uint8_t* buf )
{
        buf[0] = ASRTL_COLLECT_MSG_READY_ACK;
        return buf + 1;
}

static uint8_t* build_csrv_append(
    uint8_t*                       buf,
    asrt::flat_id                  parent_id,
    asrt::flat_id                  node_id,
    char const*                    key,
    struct asrtl_flat_value const* value )
{
        struct asrtl_span sp = { .b = buf, .e = buf + 256 };
        asrtl_msg_rtoc_collect_append(
            parent_id, node_id, key, value, asrtl_rec_span_to_span_cb, &sp );
        return sp.b;
}

struct collect_server_ctx
{
        collector coll;

        std::function< asrtl_status( asrtl_chann_id, asrtl_rec_span*, asrtl_send_done_cb, void* ) >
            send_fn{ [this](
                         asrtl_chann_id     id,
                         asrtl_rec_span*    buff,
                         asrtl_send_done_cb done_cb,
                         void*              done_ptr ) {
                    return sender_collect( &coll, id, buff, done_cb, done_ptr );
            } };

        asrtl_node head{};

        asrtc_collect_server srv;

        uint32_t t = 1;

        collect_server_ctx()
        {
                if ( asrt::init( srv, head, send_fn, asrtl_default_allocator(), 4, 16 ) !=
                     ASRTL_SUCCESS )
                        throw std::runtime_error( "collect_server init failed" );
        }

        ~collect_server_ctx() { asrt::deinit( srv ); }

        void make_active( asrt::flat_id root_id = 0 )
        {
                auto noop = []( asrtl_status ) {};
                CHECK_EQ( ASRTL_SUCCESS, asrt::send_ready( srv, root_id, noop, 1000 ) );
                coll.data.clear();
                uint8_t buf[16];
                CHECK_EQ(
                    ASRTL_SUCCESS,
                    inject_csrv_msg( &asrt::node( srv ), buf, build_csrv_ready_ack( buf ) ) );
                CHECK_EQ( ASRTL_SUCCESS, asrt::tick( asrt::node( srv ), t++ ) );
                coll.data.clear();
        }
};

TEST_CASE_FIXTURE( collect_server_ctx, "collect_server_node_chained" )
{
        CHECK_EQ( ASRTL_COLL, asrt::node( srv ).chid );
}

TEST_CASE_FIXTURE( collect_server_ctx, "collect_server_next_node_id_starts_at_1" )
{
        CHECK_EQ( 1u, asrt::next_node_id( srv ) );
}

TEST_CASE_FIXTURE( collect_server_ctx, "collect_server_send_ready_encodes" )
{
        auto noop = []( asrtl_status ) {};
        CHECK_EQ( ASRTL_SUCCESS, asrt::send_ready( srv, 5u, noop, 1000 ) );

        REQUIRE_EQ( 1u, coll.data.size() );
        auto& msg = coll.data.front();
        CHECK_EQ( ASRTL_COLL, msg.id );
        CHECK_EQ( ASRTL_COLLECT_MSG_READY, msg.data[0] );
        // root_id = 5 at offset 1..4 (LE)
        assert_u32( 5u, msg.data.data() + 1 );
        // next_node_id = 1 at offset 5..8 (LE)
        assert_u32( 1u, msg.data.data() + 5 );
        CHECK_EQ( 9u, msg.data.size() );
}

TEST_CASE_FIXTURE( collect_server_ctx, "collect_server_ready_ack_fires_cb" )
{
        int  ack_count = 0;
        auto on_ack    = [&]( asrtl_status ) {
                ++ack_count;
        };
        CHECK_EQ( ASRTL_SUCCESS, asrt::send_ready( srv, 1u, on_ack, 1000 ) );
        coll.data.clear();

        uint8_t buf[16];
        CHECK_EQ(
            ASRTL_SUCCESS,
            inject_csrv_msg( &asrt::node( srv ), buf, build_csrv_ready_ack( buf ) ) );

        CHECK_EQ( 0, ack_count );
        CHECK_EQ( ASRTL_SUCCESS, asrt::tick( asrt::node( srv ), t++ ) );
        CHECK_EQ( 1, ack_count );

        // second tick must not re-fire
        CHECK_EQ( ASRTL_SUCCESS, asrt::tick( asrt::node( srv ), t++ ) );
        CHECK_EQ( 1, ack_count );
}

TEST_CASE_FIXTURE( collect_server_ctx, "collect_server_append_builds_tree" )
{
        make_active();

        // Append: parent=0, node_id=1, key="root", type=OBJECT
        asrtl_flat_value obj_val = { .type = ASRTL_FLAT_CTYPE_OBJECT };
        uint8_t          buf[256];
        CHECK_EQ(
            ASRTL_SUCCESS,
            inject_csrv_msg(
                &asrt::node( srv ), buf, build_csrv_append( buf, 0, 1, "root", &obj_val ) ) );
        CHECK_EQ( ASRTL_SUCCESS, asrt::tick( asrt::node( srv ), t++ ) );

        // Append: parent=1, node_id=2, key="val", type=U32, value=42
        asrtl_flat_value u32_val = { .type = ASRTL_FLAT_STYPE_U32 };
        u32_val.data.s.u32_val   = 42;
        CHECK_EQ(
            ASRTL_SUCCESS,
            inject_csrv_msg(
                &asrt::node( srv ), buf, build_csrv_append( buf, 1, 2, "val", &u32_val ) ) );
        CHECK_EQ( ASRTL_SUCCESS, asrt::tick( asrt::node( srv ), t++ ) );

        // Verify tree
        auto const& tree = asrt::tree( srv );

        asrtl_flat_query_result res = {};
        CHECK_EQ( ASRTL_SUCCESS, asrtl_flat_tree_query( &tree, 1, &res ) );
        CHECK_EQ( ASRTL_FLAT_CTYPE_OBJECT, res.value.type );
        REQUIRE_NE( nullptr, res.key );
        CHECK_EQ( std::string_view{ "root" }, std::string_view{ res.key } );

        CHECK_EQ( ASRTL_SUCCESS, asrtl_flat_tree_query( &tree, 2, &res ) );
        CHECK_EQ( ASRTL_FLAT_STYPE_U32, res.value.type );
        CHECK_EQ( 42u, res.value.data.s.u32_val );
        REQUIRE_NE( nullptr, res.key );
        CHECK_EQ( std::string_view{ "val" }, std::string_view{ res.key } );
}

TEST_CASE_FIXTURE( collect_server_ctx, "collect_server_append_string_value" )
{
        make_active();

        asrtl_flat_value str_val = { .type = ASRTL_FLAT_STYPE_STR };
        str_val.data.s.str_val   = "hello";
        uint8_t buf[256];
        CHECK_EQ(
            ASRTL_SUCCESS,
            inject_csrv_msg(
                &asrt::node( srv ), buf, build_csrv_append( buf, 0, 1, "greeting", &str_val ) ) );
        CHECK_EQ( ASRTL_SUCCESS, asrtl_chann_tick( &asrt::node( srv ), t++ ) );

        auto const& tree = asrt::tree( srv );

        asrtl_flat_query_result res = {};
        CHECK_EQ( ASRTL_SUCCESS, asrtl_flat_tree_query( &tree, 1, &res ) );
        CHECK_EQ( ASRTL_FLAT_STYPE_STR, res.value.type );
        CHECK_EQ( std::string_view{ "hello" }, std::string_view{ res.value.data.s.str_val } );
}

TEST_CASE_FIXTURE( collect_server_ctx, "collect_server_duplicate_append_sends_error" )
{
        make_active();

        asrtl_flat_value u32_val = { .type = ASRTL_FLAT_STYPE_U32 };
        u32_val.data.s.u32_val   = 1;
        uint8_t buf[256];
        CHECK_EQ(
            ASRTL_SUCCESS,
            inject_csrv_msg(
                &asrt::node( srv ), buf, build_csrv_append( buf, 0, 1, "a", &u32_val ) ) );
        CHECK_EQ( ASRTL_SUCCESS, asrtl_chann_tick( &asrt::node( srv ), t++ ) );
        coll.data.clear();

        // duplicate node_id=1
        CHECK_EQ(
            ASRTL_SUCCESS,
            inject_csrv_msg(
                &asrt::node( srv ), buf, build_csrv_append( buf, 0, 1, "b", &u32_val ) ) );
        CHECK_EQ( ASRTL_SUCCESS, asrtl_chann_tick( &asrt::node( srv ), t++ ) );

        // server should send an ERROR back
        REQUIRE_GE( coll.data.size(), 1u );
        CHECK_EQ( ASRTL_COLL, coll.data.back().id );
        CHECK_EQ( ASRTL_COLLECT_MSG_ERROR, coll.data.back().data[0] );
}
