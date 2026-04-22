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
#include "./controller.h"

#include "../asrtl/asrtl_assert.h"
#include "../asrtl/core_proto.h"
#include "../asrtl/log.h"
#include "../asrtl/proto_version.h"

#include <string.h>

static inline enum asrtl_status asrtc_send( void* p, struct asrtl_rec_span* buff )
{
        struct asrtc_controller* c = (struct asrtc_controller*) p;
        ASRTL_ASSERT( c && buff && buff->b && buff->e );
        return asrtl_send( &c->sendr, ASRTL_CORE, buff, NULL, NULL );
}

static uint32_t asrtc_check_timeout( struct asrtc_controller* c, uint32_t now )
{
        if ( now >= c->deadline ) {
                c->state = ASRTC_CNTR_IDLE;
                return 1;
        }
        return 0;
}

//---------------------------------------------------------------------
// init

enum asrtl_status asrtc_cntr_start(
    struct asrtc_controller* c,
    asrtc_init_callback      cb,
    void*                    ptr,
    uint32_t                 timeout )
{
        if ( !c || !cb )
                return ASRTL_INIT_ERR;
        if ( !asrtc_cntr_idle( c ) )
                return ASRTL_BUSY_ERR;
        c->hndl.init = ( struct asrtc_init_handler ){ .cb = cb, .ptr = ptr, .timeout = timeout };
        c->state     = ASRTC_CNTR_INIT;
        c->stage     = ASRTC_STAGE_INIT;
        return ASRTL_SUCCESS;
}

void asrtc_cntr_deinit( struct asrtc_controller* c )
{
        ASRTL_ASSERT( c );
        asrtl_node_unlink( &c->node );
        if ( c->state == ASRTC_CNTR_HNDL_DESC && c->hndl.desc.desc )
                asrtl_free( &c->alloc, (void**) &c->hndl.desc.desc );
        else if ( c->state == ASRTC_CNTR_HNDL_TI && c->hndl.ti.desc )
                asrtl_free( &c->alloc, (void**) &c->hndl.ti.desc );
}

static enum asrtl_status asrtc_cntr_tick_init( struct asrtc_controller* c, uint32_t now )
{
        ASRTL_ASSERT( c->state == ASRTC_CNTR_INIT );
        struct asrtc_init_handler* h = &c->hndl.init;
        switch ( c->stage ) {
        case ASRTC_STAGE_INIT:
                if ( asrtl_msg_ctor_proto_version( asrtc_send, c ) != ASRTL_SUCCESS )
                        return ASRTL_SEND_ERR;
                c->stage    = ASRTC_STAGE_WAITING;
                c->deadline = now + h->timeout;
                break;
        case ASRTC_STAGE_WAITING:
                if ( asrtc_check_timeout( c, now ) )
                        return h->cb( h->ptr, ASRTL_TIMEOUT_ERR );
                break;
        case ASRTC_STAGE_END: {
                enum asrtl_status s;
                if ( h->ver.major != ASRTL_PROTO_MAJOR ) {
                        ASRTL_ERR_LOG(
                            "asrtc",
                            "Protocol version mismatch: got %u.%u.%u, expected %u.x.x",
                            h->ver.major,
                            h->ver.minor,
                            h->ver.patch,
                            ASRTL_PROTO_MAJOR );
                        s = ASRTL_VERSION_ERR;
                } else {
                        ASRTL_INF_LOG( "asrtc", "Controller initialized" );
                        s = ASRTL_SUCCESS;
                }
                c->state = ASRTC_CNTR_IDLE;
                return h->cb( h->ptr, s );
        }
        }
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtc_cntr_recv_init(
    struct asrtc_controller* c,
    enum asrtl_message_id_e  eid,
    struct asrtl_span*       buff )
{
        ASRTL_ASSERT( c->state == ASRTC_CNTR_INIT );

        if ( eid != ASRTL_MSG_PROTO_VERSION )
                return ASRTL_RECV_UNEXPECTED_ERR;
        if ( asrtl_span_unfit_for( buff, 3 * sizeof( uint16_t ) ) )
                return ASRTL_RECV_ERR;

        struct asrtc_init_handler* h = &c->hndl.init;
        if ( c->stage != ASRTC_STAGE_WAITING )  // XXX: can this get stuck?  // C02
                return ASRTL_RECV_INTERNAL_ERR;

        asrtl_cut_u16( &buff->b, &h->ver.major );
        asrtl_cut_u16( &buff->b, &h->ver.minor );
        asrtl_cut_u16( &buff->b, &h->ver.patch );

        c->stage = ASRTC_STAGE_END;
        return ASRTL_SUCCESS;
}

//---------------------------------------------------------------------
// test count

enum asrtl_status asrtc_cntr_test_count(
    struct asrtc_controller*  c,
    asrtc_test_count_callback cb,
    void*                     ptr,
    uint32_t                  timeout )
{
        ASRTL_ASSERT( c && cb );
        if ( !asrtc_cntr_idle( c ) )
                return ASRTL_BUSY_ERR;

        c->hndl.tc = ( struct asrtc_tc_handler ){
            .count   = 0,
            .ptr     = ptr,
            .cb      = cb,
            .timeout = timeout,
        };
        c->stage = ASRTC_STAGE_INIT;
        c->state = ASRTC_CNTR_HNDL_TC;
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtc_cntr_tick_test_count( struct asrtc_controller* c, uint32_t now )
{
        ASRTL_ASSERT( c->state == ASRTC_CNTR_HNDL_TC );
        struct asrtc_tc_handler* h = &c->hndl.tc;
        switch ( c->stage ) {
        case ASRTC_STAGE_INIT:
                if ( asrtl_msg_ctor_test_count( asrtc_send, c ) != ASRTL_SUCCESS )
                        return ASRTL_SEND_ERR;
                c->stage    = ASRTC_STAGE_WAITING;
                c->deadline = now + h->timeout;
                break;
        case ASRTC_STAGE_WAITING:
                if ( asrtc_check_timeout( c, now ) )
                        return h->cb( h->ptr, ASRTL_TIMEOUT_ERR, 0 );
                break;
        case ASRTC_STAGE_END:
                h->cb( h->ptr, ASRTL_SUCCESS, h->count );  // XXX: ignored return status - check
                                                           // everywhere here
                c->state = ASRTC_CNTR_IDLE;
        }
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtc_cntr_recv_test_count(
    struct asrtc_controller* c,
    enum asrtl_message_id_e  eid,
    struct asrtl_span*       buff )
{
        ASRTL_ASSERT( c->state == ASRTC_CNTR_HNDL_TC );

        if ( eid != ASRTL_MSG_TEST_COUNT )
                return ASRTL_RECV_UNEXPECTED_ERR;
        if ( asrtl_span_unfit_for( buff, sizeof( uint16_t ) ) )
                return ASRTL_RECV_ERR;
        struct asrtc_tc_handler* h = &c->hndl.tc;
        if ( c->stage != ASRTC_STAGE_WAITING )
                return ASRTL_RECV_INTERNAL_ERR;

        asrtl_cut_u16( &buff->b, &h->count );

        c->stage = ASRTC_STAGE_END;
        return ASRTL_SUCCESS;
}

//---------------------------------------------------------------------
// desc

enum asrtl_status asrtc_cntr_desc(
    struct asrtc_controller* c,
    asrtc_desc_callback      cb,
    void*                    ptr,
    uint32_t                 timeout )
{
        ASRTL_ASSERT( c && cb );

        if ( !asrtc_cntr_idle( c ) )
                return ASRTL_BUSY_ERR;

        c->hndl.desc = ( struct asrtc_desc_handler ){
            .desc    = NULL,
            .ptr     = ptr,
            .cb      = cb,
            .timeout = timeout,
        };
        c->stage = ASRTC_STAGE_INIT;
        c->state = ASRTC_CNTR_HNDL_DESC;
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtc_cntr_tick_desc( struct asrtc_controller* c, uint32_t now )
{
        ASRTL_ASSERT( c->state == ASRTC_CNTR_HNDL_DESC );
        struct asrtc_desc_handler* h = &c->hndl.desc;
        switch ( c->stage ) {
        case ASRTC_STAGE_INIT:
                if ( asrtl_msg_ctor_desc( asrtc_send, c ) != ASRTL_SUCCESS )
                        return ASRTL_SEND_ERR;
                c->stage    = ASRTC_STAGE_WAITING;
                c->deadline = now + h->timeout;
                break;
        case ASRTC_STAGE_WAITING:
                if ( asrtc_check_timeout( c, now ) )
                        return h->cb( h->ptr, ASRTL_TIMEOUT_ERR, NULL );
                break;
        case ASRTC_STAGE_END: {
                enum asrtl_status res = h->cb( h->ptr, ASRTL_SUCCESS, h->desc );
                asrtl_free( &c->alloc, (void**) &h->desc );
                c->state = ASRTC_CNTR_IDLE;
                return res;
        }
        }
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtc_cntr_recv_desc(
    struct asrtc_controller* c,
    enum asrtl_message_id_e  e,
    struct asrtl_span*       buff )
{
        ASRTL_ASSERT( c->state == ASRTC_CNTR_HNDL_DESC );
        if ( e != ASRTL_MSG_DESC )
                return ASRTL_RECV_UNEXPECTED_ERR;

        struct asrtc_desc_handler* h = &c->hndl.desc;
        if ( c->stage != ASRTC_STAGE_WAITING )
                return ASRTL_RECV_INTERNAL_ERR;

        h->desc = asrtl_realloc_str( &c->alloc, buff );
        if ( h->desc == NULL )
                return ASRTL_ALLOC_ERR;
        c->stage = ASRTC_STAGE_END;

        return ASRTL_SUCCESS;
}

//---------------------------------------------------------------------
// test info

enum asrtl_status asrtc_cntr_test_info(
    struct asrtc_controller* c,
    uint16_t                 id,
    asrtc_test_info_callback cb,
    void*                    ptr,
    uint32_t                 timeout )
{
        ASRTL_ASSERT( c && cb );
        if ( !asrtc_cntr_idle( c ) )
                return ASRTL_BUSY_ERR;

        c->hndl.ti = ( struct asrtc_ti_handler ){
            .tid     = id,
            .desc    = NULL,
            .ptr     = ptr,
            .cb      = cb,
            .timeout = timeout,
        };
        c->stage = ASRTC_STAGE_INIT;
        c->state = ASRTC_CNTR_HNDL_TI;
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtc_cntr_tick_test_info( struct asrtc_controller* c, uint32_t now )
{
        ASRTL_ASSERT( c->state == ASRTC_CNTR_HNDL_TI );
        struct asrtc_ti_handler* h = &c->hndl.ti;
        switch ( c->stage ) {
        case ASRTC_STAGE_INIT:
                if ( asrtl_msg_ctor_test_info( h->tid, asrtc_send, c ) != ASRTL_SUCCESS )
                        return ASRTL_SEND_ERR;
                c->stage    = ASRTC_STAGE_WAITING;
                c->deadline = now + h->timeout;
                break;
        case ASRTC_STAGE_WAITING:
                if ( asrtc_check_timeout( c, now ) )
                        return h->cb( h->ptr, ASRTL_TIMEOUT_ERR, h->tid, NULL );
                break;
        case ASRTC_STAGE_END: {
                enum asrtl_status res = h->cb( h->ptr, ASRTL_SUCCESS, h->tid, h->desc );
                asrtl_free( &c->alloc, (void**) &h->desc );
                c->state = ASRTC_CNTR_IDLE;
                return res;
        }
        }
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtc_cntr_recv_test_info(
    struct asrtc_controller* c,
    enum asrtl_message_id_e  eid,
    struct asrtl_span*       buff )
{
        ASRTL_ASSERT( c->state == ASRTC_CNTR_HNDL_TI );
        if ( eid != ASRTL_MSG_TEST_INFO )
                return ASRTL_RECV_UNEXPECTED_ERR;

        if ( asrtl_span_unfit_for( buff, sizeof( uint16_t ) ) )
                return ASRTL_RECV_ERR;

        struct asrtc_ti_handler* h = &c->hndl.ti;
        if ( c->stage != ASRTC_STAGE_WAITING )
                return ASRTL_RECV_INTERNAL_ERR;
        uint16_t tid;
        asrtl_cut_u16( &buff->b, &tid );
        if ( tid != h->tid ) {
                ASRTL_ERR_LOG(
                    "asrtc", "Test info response tid mismatch: got %u, expected %u", tid, h->tid );
                return ASRTL_RECV_UNEXPECTED_ERR;
        }

        h->desc = asrtl_realloc_str( &c->alloc, buff );
        if ( h->desc == NULL )
                return ASRTL_ALLOC_ERR;
        c->stage = ASRTC_STAGE_END;

        return ASRTL_SUCCESS;
}

//---------------------------------------------------------------------
// test info

enum asrtl_status asrtc_cntr_test_exec(
    struct asrtc_controller*   c,
    uint16_t                   id,
    asrtc_test_result_callback cb,
    void*                      ptr,
    uint32_t                   timeout )
{
        ASRTL_ASSERT( c && cb );
        if ( !asrtc_cntr_idle( c ) )
                return ASRTL_BUSY_ERR;
        c->hndl.exec = ( struct asrtc_exec_handler ){
            .res =
                ( struct asrtc_result ){
                    .test_id = id,
                    .run_id  = c->run_id++,
                    .res     = ASRTC_TEST_UNKNOWN,
                },
            .ptr     = ptr,
            .cb      = cb,
            .timeout = timeout,
        };
        c->stage = ASRTC_STAGE_INIT;
        c->state = ASRTC_CNTR_HNDL_EXEC;
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtc_cntr_tick_test_exec( struct asrtc_controller* c, uint32_t now )
{
        struct asrtc_exec_handler* h = &c->hndl.exec;
        switch ( c->stage ) {
        case ASRTC_STAGE_INIT:
                if ( asrtl_msg_ctor_test_start( h->res.test_id, h->res.run_id, asrtc_send, c ) !=
                     ASRTL_SUCCESS )
                        return ASRTL_SEND_ERR;
                c->stage    = ASRTC_STAGE_WAITING;
                c->deadline = now + h->timeout;
                break;
        case ASRTC_STAGE_WAITING:
                if ( asrtc_check_timeout( c, now ) )
                        return h->cb( h->ptr, ASRTL_TIMEOUT_ERR, &h->res );
                break;
        case ASRTC_STAGE_END: {
                enum asrtl_status res = h->cb( h->ptr, ASRTL_SUCCESS, &h->res );
                c->state              = ASRTC_CNTR_IDLE;
                return res;
        }
        }
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtc_cntr_recv_test_exec(
    struct asrtc_controller* c,
    enum asrtl_message_id_e  eid,
    struct asrtl_span*       buff )
{
        ASRTL_ASSERT( c->state == ASRTC_CNTR_HNDL_EXEC );
        struct asrtc_exec_handler* h = &c->hndl.exec;
        if ( c->stage != ASRTC_STAGE_WAITING )
                return ASRTL_RECV_INTERNAL_ERR;
        switch ( eid ) {
        case ASRTL_MSG_TEST_START: {
                if ( asrtl_span_unfit_for( buff, sizeof( uint16_t ) + sizeof( uint32_t ) ) )
                        return ASRTL_RECV_ERR;
                // XXX should not be ignored
                uint16_t* tid = &h->res.test_id;
                asrtl_cut_u16( &buff->b, tid );
                // XXX: should not be ignored
                uint32_t* rid = &h->res.run_id;
                asrtl_cut_u32( &buff->b, rid );
                break;
        }
        case ASRTL_MSG_TEST_RESULT: {
                if ( asrtl_span_unfit_for(
                         buff, sizeof( uint32_t ) + sizeof( asrtl_test_result ) ) )
                        return ASRTL_RECV_ERR;
                uint32_t rid;
                // XXX: should not be ignored
                asrtl_cut_u32( &buff->b, &rid );
                uint16_t res;
                // XXX: should not be ignored
                asrtl_cut_u16( &buff->b, &res );

                if ( rid != h->res.run_id ) {
                        ASRTL_ERR_LOG(
                            "asrtc_main",
                            "Received test result for unexpected run: %u (expected %u)",
                            rid,
                            h->res.run_id );
                        h->res.res = ASRTC_TEST_ERROR;
                } else {
                        ASRTL_INF_LOG( "asrtc_main", "Received test result: %u", res );
                        h->res.res = res;
                }

                c->stage = ASRTC_STAGE_END;
                break;
        }
        default:
                return ASRTL_RECV_UNEXPECTED_ERR;
        }

        return ASRTL_SUCCESS;
}

//---------------------------------------------------------------------
// tick

enum asrtl_status asrtc_cntr_tick( struct asrtc_controller* c, uint32_t now )
{
        switch ( c->state ) {
        case ASRTC_CNTR_INIT:
                return asrtc_cntr_tick_init( c, now );
        case ASRTC_CNTR_HNDL_TC:
                return asrtc_cntr_tick_test_count( c, now );
        case ASRTC_CNTR_HNDL_DESC:
                return asrtc_cntr_tick_desc( c, now );
        case ASRTC_CNTR_HNDL_TI:
                return asrtc_cntr_tick_test_info( c, now );
        case ASRTC_CNTR_HNDL_EXEC:
                return asrtc_cntr_tick_test_exec( c, now );
        case ASRTC_CNTR_IDLE:
                break;
        }
        return ASRTL_SUCCESS;
}

uint32_t asrtc_cntr_idle( struct asrtc_controller const* c )
{
        return c->state == ASRTC_CNTR_IDLE;
}

static enum asrtl_status asrtc_cntr_recv_error( struct asrtc_error_cb* h, struct asrtl_span* buff )
{
        if ( asrtl_span_unfit_for( buff, sizeof( uint16_t ) ) )
                return ASRTL_RECV_ERR;
        uint16_t ecode;
        asrtl_cut_u16( &buff->b, &ecode );

        if ( asrtc_raise_error( h, ASRTL_REACTOR, ecode ) != ASRTL_SUCCESS )
                return ASRTL_RECV_ERR;
        return ASRTL_SUCCESS;
}

static enum asrtl_status asrtc_cntr_recv( void* data, struct asrtl_span buff )
{
        ASRTL_ASSERT( data );
        struct asrtc_controller* c = (struct asrtc_controller*) data;
        asrtl_message_id         id;
        if ( asrtl_span_unfit_for( &buff, sizeof( asrtl_message_id ) ) )
                return ASRTL_RECV_ERR;
        asrtl_cut_u16( &buff.b, &id );

        enum asrtl_message_id_e eid = (enum asrtl_message_id_e) id;
        enum asrtl_status       st  = ASRTL_SUCCESS;
        if ( eid == ASRTL_MSG_ERROR )
                st = asrtc_cntr_recv_error( &c->eh, &buff );
        else
                switch ( c->state ) {
                case ASRTC_CNTR_INIT:
                        st = asrtc_cntr_recv_init( c, eid, &buff );
                        break;
                case ASRTC_CNTR_HNDL_TC:
                        st = asrtc_cntr_recv_test_count( c, eid, &buff );
                        break;
                case ASRTC_CNTR_HNDL_DESC:
                        st = asrtc_cntr_recv_desc( c, eid, &buff );
                        break;
                case ASRTC_CNTR_HNDL_TI:
                        st = asrtc_cntr_recv_test_info( c, eid, &buff );
                        break;
                case ASRTC_CNTR_HNDL_EXEC:
                        st = asrtc_cntr_recv_test_exec( c, eid, &buff );
                        break;
                case ASRTC_CNTR_IDLE:
                        return ASRTL_RECV_UNEXPECTED_ERR;
                }
        if ( st != ASRTL_SUCCESS )
                return st;
        return buff.b == buff.e ? ASRTL_SUCCESS : ASRTL_RECV_TRAILING_ERR;
}

static enum asrtl_status asrtc_cntr_event( void* p, enum asrtl_event_e e, void* arg )
{
        struct asrtc_controller* c = (struct asrtc_controller*) p;
        switch ( e ) {
        case ASRTL_EVENT_TICK:
                return asrtc_cntr_tick( c, *(uint32_t*) arg );
        case ASRTL_EVENT_RECV:
                return asrtc_cntr_recv( c, *(struct asrtl_span*) arg );
        }
        ASRTL_ERR_LOG( "asrtc", "unexpected event: %s", asrtl_event_to_str( e ) );
        return ASRTL_INVALID_EVENT_ERR;
}

enum asrtl_status asrtc_cntr_init(
    struct asrtc_controller* c,
    struct asrtl_sender      s,
    struct asrtl_allocator   alloc,
    struct asrtc_error_cb    eh )
{
        if ( !c || !eh.cb )
                return ASRTL_INIT_ERR;
        *c = ( struct asrtc_controller ){
            .node =
                ( struct asrtl_node ){
                    .chid     = ASRTL_CORE,
                    .e_cb_ptr = c,
                    .e_cb     = &asrtc_cntr_event,
                    .next     = NULL,
                },
            .sendr    = s,
            .alloc    = alloc,
            .eh       = eh,
            .run_id   = 0,
            .state    = ASRTC_CNTR_IDLE,
            .stage    = ASRTC_STAGE_INIT,
            .deadline = 0,
        };

        return ASRTL_SUCCESS;
}
