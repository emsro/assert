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

#include "../asrtc/cntr_assm.h"
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
#include "./util.hpp"

#include <doctest/doctest.h>
#include <format>
#include <string>
#include <vector>

static ASRT_DEFINE_GPOS_LOG()

    namespace
{
}

// ---------------------------------------------------------------------------
// helpers

/// Drain all requests from sq, find the matching node in peer_head's chain by chid,
/// and deliver each message directly via asrt_chann_recv.
static void deliver( asrt_send_req_list* sq, asrt_node* peer_head )
{
        while ( sq->head ) {
                asrt_send_req* req = sq->head;
                sq->head           = req->next;
                if ( !sq->head )
                        sq->tail = nullptr;
                req->next = nullptr;

                uint8_t  flat[512];
                uint8_t* p = flat;
                memcpy( p, req->buff.b, (size_t) ( req->buff.e - req->buff.b ) );
                p += req->buff.e - req->buff.b;
                for ( uint32_t i = 0; i < req->buff.rest_count; i++ ) {
                        size_t sz = (size_t) ( req->buff.rest[i].e - req->buff.rest[i].b );
                        memcpy( p, req->buff.rest[i].b, sz );
                        p += sz;
                }

                asrt_node*       target = asrt_chann_find( peer_head, req->chid );
                enum asrt_status st =
                    target ? asrt_chann_recv( target, { flat, p } ) : ASRT_CHANN_NOT_FOUND;
                if ( req->done_cb )
                        req->done_cb( req->done_ptr, st );
        }
}

// ---------------------------------------------------------------------------
// test callables for the reactor side

namespace
{

struct passing_test
{
        char const* name() const { return "passing_test"; }
        asrt_status operator()( asrt::record& rec )
        {
                rec.state = ASRT_TEST_PASS;
                return ASRT_SUCCESS;
        }
};

struct failing_test
{
        char const* name() const { return "failing_test"; }
        asrt_status operator()( asrt::record& rec )
        {
                rec.state = ASRT_TEST_FAIL;
                return ASRT_SUCCESS;
        }
};

// ---------------------------------------------------------------------------
// paired fixture: controller <-> reactor wired in-process

struct paired_ctx
{
        int          init_cb_count = 0;
        asrt::status init_status   = ASRT_SUCCESS;
        uint32_t     t             = 1;

        asrt::unit< passing_test > t0;
        asrt::unit< failing_test > t1;

        // Outgoing send queues — must be declared before r/c so they are valid when init runs.
        asrt_send_req_list r_send_queue = {};
        asrt_send_req_list c_send_queue = {};

        asrt_reactor    r;
        asrt_controller c;

        paired_ctx()
        {
                if ( asrt::init( r, r_send_queue, "paired_reactor" ) != ASRT_SUCCESS )
                        throw std::runtime_error( "reactor init failed" );

                REQUIRE( asrt::add_test( r, t0 ) == ASRT_SUCCESS );
                REQUIRE( asrt::add_test( r, t1 ) == ASRT_SUCCESS );

                if ( asrt::init( c, &c_send_queue, asrt_default_allocator() ) != ASRT_SUCCESS )
                        throw std::runtime_error( "controller init failed" );

                // start init handshake
                REQUIRE(
                    asrt::start(
                        c,
                        { []( void* self, asrt::status s ) -> asrt::status {
                                 auto* p = static_cast< paired_ctx* >( self );
                                 p->init_cb_count++;
                                 p->init_status = s;
                                 return ASRT_SUCCESS;
                         },
                          this },
                        1000 ) == ASRT_SUCCESS );

                // complete PROTO_VERSION handshake
                for ( int i = 0; i < 100 && !asrt::is_idle( c ); i++ ) {
                        (void) asrt::tick( asrt::node( c ), t++ );  // tick status checked
                                                                    // implicitly via is_idle
                        deliver( &c_send_queue, &r.node );
                        (void) asrt::tick( asrt::node( r ), t++ );
                        deliver( &r_send_queue, &c.node );
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
                        (void) asrt::tick( asrt::node( c ), t++ );  // tick status checked
                                                                    // implicitly via is_idle
                        deliver( &c_send_queue, &r.node );
                        (void) asrt::tick( asrt::node( r ), t++ );
                        deliver( &r_send_queue, &c.node );
                }
                CHECK( asrt::is_idle( c ) );
        }
};

}  // namespace

// ---------------------------------------------------------------------------
// fmt

TEST_CASE( "fmt_success" )
{
        std::string s = std::format( "{}", ASRT_SUCCESS );
        CHECK_EQ( s, asrt_status_to_str( ASRT_SUCCESS ) );
}

TEST_CASE( "fmt_error" )
{
        std::string s = std::format( "{}", ASRT_INIT_ERR );
        CHECK_EQ( s, asrt_status_to_str( ASRT_INIT_ERR ) );
}

// ---------------------------------------------------------------------------
// controller init

TEST_CASE_FIXTURE( paired_ctx, "controller_init" )
{
        CHECK( asrt::is_idle( c ) );
        CHECK_EQ( ASRT_CORE, asrt::node( c ).chid );
        CHECK_EQ( ASRT_SUCCESS, init_status );
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
            { []( void* self, asrt::status s, char const* desc ) -> asrt::status {
                     auto* p = static_cast< std::string* >( self );
                     CHECK_EQ( ASRT_SUCCESS, s );
                     *p = std::string{ desc };
                     return ASRT_SUCCESS;
             },
              &received },
            1000 );
        CHECK_EQ( ASRT_SUCCESS, st );
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
                     CHECK_EQ( ASRT_SUCCESS, s );
                     auto* count_ptr = static_cast< uint32_t* >( p );
                     *count_ptr      = n;
                     return ASRT_SUCCESS;
             },
                 &count },
            1000 );
        CHECK_EQ( ASRT_SUCCESS, st );
        spin();
        CHECK_EQ( 2U, count );
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
            { []( void* p, asrt::status s, uint16_t t, char const* desc ) -> asrt::status {
                     auto* ctx = static_cast< struct ctx* >( p );
                     CHECK_EQ( ASRT_SUCCESS, s );
                     *ctx->tid  = t;
                     *ctx->name = std::string{ desc };
                     return ASRT_SUCCESS;
             },
              &cb_ctx },
            1000 );
        CHECK_EQ( ASRT_SUCCESS, st );
        spin();
        CHECK_EQ( 0U, tid );
        CHECK_EQ( "passing_test", name );
}

// ---------------------------------------------------------------------------
// exec_test

TEST_CASE_FIXTURE( paired_ctx, "exec_test_pass" )
{
        asrt_test_result res = ASRT_TEST_RESULT_ERROR;
        asrt::status     st  = asrt::exec_test(
            c,
            0,
            { []( void* p, asrt::status s, asrt_result* r ) -> asrt::status {
                     auto* out = static_cast< asrt_test_result* >( p );
                     CHECK_EQ( ASRT_SUCCESS, s );
                     *out = r->res;
                     return ASRT_SUCCESS;
             },
                   &res },
            1000 );
        CHECK_EQ( ASRT_SUCCESS, st );
        spin();
        CHECK_EQ( ASRT_TEST_RESULT_SUCCESS, res );
}

TEST_CASE_FIXTURE( paired_ctx, "exec_test_fail" )
{
        asrt_test_result res = ASRT_TEST_RESULT_ERROR;
        asrt::status     st  = asrt::exec_test(
            c,
            1,
            { []( void* p, asrt::status s, asrt_result* r ) -> asrt::status {
                     auto* out = static_cast< asrt_test_result* >( p );
                     CHECK_EQ( ASRT_SUCCESS, s );
                     *out = r->res;
                     return ASRT_SUCCESS;
             },
                   &res },
            1000 );
        CHECK_EQ( ASRT_SUCCESS, st );
        spin();
        CHECK_EQ( ASRT_TEST_RESULT_FAILURE, res );
}

// ---------------------------------------------------------------------------
// busy_error: second query while controller is busy must fail and NOT store its callback

TEST_CASE_FIXTURE( paired_ctx, "busy_error" )
{
        bool first_called  = false;
        bool second_called = false;

        // start the first query — succeeds, callback stored
        CHECK_EQ(
            ASRT_SUCCESS,
            asrt::query_desc(
                c,
                { []( void* p, asrt::status, char const* ) -> asrt::status {
                         auto* called = static_cast< bool* >( p );
                         *called      = true;
                         return ASRT_SUCCESS;
                 },
                  &first_called },
                1000 ) );

        // controller is now busy — second query must be rejected; its callback must NOT be stored
        CHECK_EQ(
            ASRT_BUSY_ERR,
            asrt::query_test_count(
                c,
                { []( void* p, asrt::status, uint16_t ) -> asrt::status {
                         auto* called = static_cast< bool* >( p );
                         *called      = true;
                         return ASRT_SUCCESS;
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

namespace
{

struct diag_paired_ctx : paired_ctx
{
        asrt_diag_server c_diag;
        asrt_diag_client r_diag;

        diag_paired_ctx()
        {
                if ( asrt::init( c_diag, asrt::node( this->c ), asrt_default_allocator() ) !=
                     ASRT_SUCCESS )
                        throw std::runtime_error( "c_diag init failed" );
                if ( asrt::init( r_diag, asrt::node( this->r ) ) != ASRT_SUCCESS )
                        throw std::runtime_error( "r_diag init failed" );
        }

        ~diag_paired_ctx()
        {
                asrt::deinit( r_diag );
                asrt::deinit( c_diag );
        }
};

}  // namespace

TEST_CASE_FIXTURE( diag_paired_ctx, "diag_record_received_by_controller" )
{
        auto noop_done = []( asrt_status ) {};
        asrt::rec_diag( r_diag, "test_file.c", 42, nullptr, noop_done );
        deliver( &r_send_queue, &c.node );

        auto rec = asrt::take_record( c_diag );
        REQUIRE( rec != nullptr );
        CHECK_EQ( 42U, rec->line );
        CHECK( std::string_view{ rec->file }.ends_with( "test_file.c" ) );

        // no more records
        CHECK( asrt::take_record( c_diag ) == nullptr );
}

TEST_CASE_FIXTURE( diag_paired_ctx, "diag_multiple_records_queued_in_order" )
{
        auto noop_done = []( asrt_status ) {};
        asrt::rec_diag( r_diag, "a.c", 1, nullptr, noop_done );
        asrt::rec_diag( r_diag, "b.c", 2, nullptr, noop_done );
        asrt::rec_diag( r_diag, "c.c", 3, nullptr, noop_done );
        deliver( &r_send_queue, &c.node );

        auto r1 = asrt::take_record( c_diag );
        auto r2 = asrt::take_record( c_diag );
        REQUIRE( r1 );
        REQUIRE( !r2 );

        CHECK_EQ( 1U, r1->line );

        CHECK( asrt::take_record( c_diag ) == nullptr );
}

TEST_CASE_FIXTURE( diag_paired_ctx, "diag_independent_of_controller_queries" )
{
        // both a controller query and a diag record in flight at the same time
        std::string desc;
        auto        on_desc = [&]( asrt::status s, char const* sv ) -> asrt::status {
                CHECK_EQ( ASRT_SUCCESS, s );
                desc = std::string{ sv };
                return ASRT_SUCCESS;
        };
        CHECK_EQ( ASRT_SUCCESS, asrt::query_desc( c, on_desc, 1000 ) );

        auto noop_done = []( asrt_status ) {};
        asrt::rec_diag( r_diag, "diag_test.c", 99, nullptr, noop_done );

        spin();

        CHECK_EQ( "paired_reactor", desc );

        auto rec = asrt::take_record( c_diag );
        REQUIRE( rec != nullptr );
        CHECK_EQ( 99U, rec->line );
}

// ---------------------------------------------------------------------------
// param_server wrapper

namespace
{

struct param_server_ctx : paired_ctx
{
        collector         param_coll;
        asrt_param_server srv;

        param_server_ctx()
        {
                if ( asrt::init( srv, asrt::node( c ), asrt_default_allocator() ) != ASRT_SUCCESS )
                        throw std::runtime_error( "param_server init failed" );
        }

        ~param_server_ctx() { asrt::deinit( srv ); }

        void drain_param() { drain_send_queue( &c_send_queue, &param_coll ); }
};

}  // namespace

TEST_CASE_FIXTURE( param_server_ctx, "param_server_node_chained" )
{
        CHECK_EQ( ASRT_PARA, asrt::node( srv ).chid );
}

TEST_CASE_FIXTURE( param_server_ctx, "param_server_set_tree_and_send_ready" )
{
        struct asrt_flat_tree tree;
        asrt_flat_tree_init( &tree, asrt_default_allocator(), 4, 16 );
        asrt_flat_tree_append_cont( &tree, 0, 1, nullptr, ASRT_FLAT_CTYPE_OBJECT );
        asrt_flat_tree_append_scalar( &tree, 1, 2, "k", ASRT_FLAT_STYPE_U32, { .u32_val = 42 } );

        asrt::set_tree( srv, tree );
        auto noop = []( asrt_status ) {};
        CHECK_EQ( ASRT_SUCCESS, asrt::send_ready( srv, 1U, noop, 1000 ) );
        drain_param();

        REQUIRE_EQ( 1U, param_coll.data.size() );
        CHECK_EQ( ASRT_PARA, param_coll.data.front().id );
        CHECK_EQ( ASRT_PARAM_MSG_READY, param_coll.data.front().data[0] );
        param_coll.data.pop_front();

        asrt_flat_tree_deinit( &tree );
}

TEST_CASE_FIXTURE( param_server_ctx, "param_server_ready_ack_cb_fires" )
{
        struct asrt_flat_tree tree;
        asrt_flat_tree_init( &tree, asrt_default_allocator(), 4, 16 );
        asrt_flat_tree_append_cont( &tree, 0, 1, nullptr, ASRT_FLAT_CTYPE_OBJECT );

        asrt::set_tree( srv, tree );

        int  ack_count = 0;
        auto on_ack    = [&]( asrt_status ) {
                ++ack_count;
        };
        CHECK_EQ( ASRT_SUCCESS, asrt::send_ready( srv, 1U, on_ack, 1000 ) );
        drain_param();
        param_coll.data.clear();

        // Build a READY_ACK message: [ASRT_PARAM_MSG_READY_ACK, max_msg_size(256) LE]
        uint8_t   ack_msg[5] = { ASRT_PARAM_MSG_READY_ACK, 0, 1, 0, 0 };  // 256 LE
        asrt_span sp         = { .b = ack_msg, .e = ack_msg + sizeof ack_msg };

        auto& n = asrt::node( srv );
        asrt::recv( n, sp );

        CHECK_EQ( 0, ack_count );  // not yet — pending, needs tick
        CHECK_EQ( ASRT_SUCCESS, asrt::tick( asrt::node( srv ), t++ ) );
        CHECK_EQ( 1, ack_count );  // fired once

        // Second tick should not fire again
        CHECK_EQ( ASRT_SUCCESS, asrt::tick( asrt::node( srv ), t++ ) );
        CHECK_EQ( 1, ack_count );

        asrt_flat_tree_deinit( &tree );
}

// ---------------------------------------------------------------------------
// collect_server wrapper
// ---------------------------------------------------------------------------

static inline asrt_status inject_csrv_msg( asrt_node* n, uint8_t* b, uint8_t* e )
{
        return asrt_chann_recv( n, ( asrt_span ){ .b = b, .e = e } );
}

static uint8_t* build_csrv_ready_ack( uint8_t* buf )
{
        buf[0] = ASRT_COLLECT_MSG_READY_ACK;
        return buf + 1;
}

static uint8_t* build_csrv_append(
    uint8_t*                      buf,
    asrt::flat_id                 parent_id,
    asrt::flat_id                 node_id,
    char const*                   key,
    struct asrt_flat_value const* value )
{
        struct asrt_collect_append_msg amsg = {};
        struct asrt_send_req*          req =
            asrt_msg_rtoc_collect_append( &amsg, parent_id, node_id, key, value );
        uint8_t* p = buf;
        memcpy( p, req->buff.b, (size_t) ( req->buff.e - req->buff.b ) );
        p += req->buff.e - req->buff.b;
        for ( uint32_t i = 0; i < req->buff.rest_count; i++ ) {
                size_t sz = (size_t) ( req->buff.rest[i].e - req->buff.rest[i].b );
                memcpy( p, req->buff.rest[i].b, sz );
                p += sz;
        }
        return p;
}

namespace
{

struct collect_server_ctx
{
        collector           coll;
        asrt_send_req_list  sq = {};
        asrt_node           head{};
        asrt_collect_server srv;
        uint32_t            t = 1;

        collect_server_ctx()
        {
                head.send_queue = &sq;
                if ( asrt::init( srv, head, asrt_default_allocator(), 4, 16 ) != ASRT_SUCCESS )
                        throw std::runtime_error( "collect_server init failed" );
        }

        ~collect_server_ctx() { asrt::deinit( srv ); }

        void drain() { drain_send_queue( &sq, &coll ); }

        void make_active( asrt::flat_id root_id = 0 )
        {
                auto noop = []( asrt_status ) {};
                CHECK_EQ( ASRT_SUCCESS, asrt::send_ready( srv, root_id, noop, 1000 ) );
                drain();
                coll.data.clear();
                uint8_t buf[16];
                CHECK_EQ(
                    ASRT_SUCCESS,
                    inject_csrv_msg( &asrt::node( srv ), buf, build_csrv_ready_ack( buf ) ) );
                CHECK_EQ( ASRT_SUCCESS, asrt::tick( asrt::node( srv ), t++ ) );
                coll.data.clear();
        }
};

}  // namespace

TEST_CASE_FIXTURE( collect_server_ctx, "collect_server_node_chained" )
{
        CHECK_EQ( ASRT_COLL, asrt::node( srv ).chid );
}

TEST_CASE_FIXTURE( collect_server_ctx, "collect_server_next_node_id_starts_at_1" )
{
        CHECK_EQ( 1U, asrt::next_node_id( srv ) );
}

TEST_CASE_FIXTURE( collect_server_ctx, "collect_server_send_ready_encodes" )
{
        auto noop = []( asrt_status ) {};
        CHECK_EQ( ASRT_SUCCESS, asrt::send_ready( srv, 5U, noop, 1000 ) );
        drain();

        REQUIRE_EQ( 1U, coll.data.size() );
        auto& msg = coll.data.front();
        CHECK_EQ( ASRT_COLL, msg.id );
        CHECK_EQ( ASRT_COLLECT_MSG_READY, msg.data[0] );
        // root_id = 5 at offset 1..4 (LE)
        assert_u32( 5U, msg.data.data() + 1 );
        // next_node_id = 1 at offset 5..8 (LE)
        assert_u32( 1U, msg.data.data() + 5 );
        CHECK_EQ( 9U, msg.data.size() );
}

TEST_CASE_FIXTURE( collect_server_ctx, "collect_server_ready_ack_fires_cb" )
{
        int  ack_count = 0;
        auto on_ack    = [&]( asrt_status ) {
                ++ack_count;
        };
        CHECK_EQ( ASRT_SUCCESS, asrt::send_ready( srv, 1U, on_ack, 1000 ) );
        drain();
        coll.data.clear();

        uint8_t buf[16];
        CHECK_EQ(
            ASRT_SUCCESS, inject_csrv_msg( &asrt::node( srv ), buf, build_csrv_ready_ack( buf ) ) );

        CHECK_EQ( 0, ack_count );
        CHECK_EQ( ASRT_SUCCESS, asrt::tick( asrt::node( srv ), t++ ) );
        CHECK_EQ( 1, ack_count );

        // second tick must not re-fire
        CHECK_EQ( ASRT_SUCCESS, asrt::tick( asrt::node( srv ), t++ ) );
        CHECK_EQ( 1, ack_count );
}

TEST_CASE_FIXTURE( collect_server_ctx, "collect_server_append_builds_tree" )
{
        make_active();

        // Append: parent=0, node_id=1, key="root", type=OBJECT
        asrt_flat_value obj_val = { .type = ASRT_FLAT_CTYPE_OBJECT };
        uint8_t         buf[256];
        CHECK_EQ(
            ASRT_SUCCESS,
            inject_csrv_msg(
                &asrt::node( srv ), buf, build_csrv_append( buf, 0, 1, "root", &obj_val ) ) );
        CHECK_EQ( ASRT_SUCCESS, asrt::tick( asrt::node( srv ), t++ ) );

        // Append: parent=1, node_id=2, key="val", type=U32, value=42
        asrt_flat_value u32_val = { .type = ASRT_FLAT_STYPE_U32 };
        u32_val.data.s.u32_val  = 42;
        CHECK_EQ(
            ASRT_SUCCESS,
            inject_csrv_msg(
                &asrt::node( srv ), buf, build_csrv_append( buf, 1, 2, "val", &u32_val ) ) );
        CHECK_EQ( ASRT_SUCCESS, asrt::tick( asrt::node( srv ), t++ ) );

        // Verify tree
        auto const& tree = asrt::tree( srv );

        asrt_flat_query_result res = {};
        CHECK_EQ( ASRT_SUCCESS, asrt_flat_tree_query( &tree, 1, &res ) );
        CHECK_EQ( ASRT_FLAT_CTYPE_OBJECT, res.value.type );
        REQUIRE_NE( nullptr, res.key );
        CHECK_EQ( std::string_view{ "root" }, std::string_view{ res.key } );

        CHECK_EQ( ASRT_SUCCESS, asrt_flat_tree_query( &tree, 2, &res ) );
        CHECK_EQ( ASRT_FLAT_STYPE_U32, res.value.type );
        CHECK_EQ( 42U, res.value.data.s.u32_val );
        REQUIRE_NE( nullptr, res.key );
        CHECK_EQ( std::string_view{ "val" }, std::string_view{ res.key } );
}

TEST_CASE_FIXTURE( collect_server_ctx, "collect_server_append_string_value" )
{
        make_active();

        asrt_flat_value str_val = { .type = ASRT_FLAT_STYPE_STR };
        str_val.data.s.str_val  = "hello";
        uint8_t buf[256];
        CHECK_EQ(
            ASRT_SUCCESS,
            inject_csrv_msg(
                &asrt::node( srv ), buf, build_csrv_append( buf, 0, 1, "greeting", &str_val ) ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &asrt::node( srv ), t++ ) );

        auto const& tree = asrt::tree( srv );

        asrt_flat_query_result res = {};
        CHECK_EQ( ASRT_SUCCESS, asrt_flat_tree_query( &tree, 1, &res ) );
        CHECK_EQ( ASRT_FLAT_STYPE_STR, res.value.type );
        CHECK_EQ( std::string_view{ "hello" }, std::string_view{ res.value.data.s.str_val } );
}

TEST_CASE_FIXTURE( collect_server_ctx, "collect_server_duplicate_append_sends_error" )
{
        make_active();

        asrt_flat_value u32_val = { .type = ASRT_FLAT_STYPE_U32 };
        u32_val.data.s.u32_val  = 1;
        uint8_t buf[256];
        CHECK_EQ(
            ASRT_SUCCESS,
            inject_csrv_msg(
                &asrt::node( srv ), buf, build_csrv_append( buf, 0, 1, "a", &u32_val ) ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &asrt::node( srv ), t++ ) );
        drain();
        coll.data.clear();

        // duplicate node_id=1
        CHECK_EQ(
            ASRT_SUCCESS,
            inject_csrv_msg(
                &asrt::node( srv ), buf, build_csrv_append( buf, 0, 1, "b", &u32_val ) ) );
        CHECK_EQ( ASRT_SUCCESS, asrt_chann_tick( &asrt::node( srv ), t++ ) );
        drain();

        // server should send an ERROR back
        REQUIRE_GE( coll.data.size(), 1U );
        CHECK_EQ( ASRT_COLL, coll.data.back().id );
        CHECK_EQ( ASRT_COLLECT_MSG_ERROR, coll.data.back().data[0] );
}
