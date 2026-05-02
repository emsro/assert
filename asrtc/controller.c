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

#include "../asrtl/asrt_assert.h"
#include "../asrtl/core_proto.h"
#include "../asrtl/log.h"
#include "../asrtl/proto_version.h"

#include <string.h>

static uint32_t asrt_cntr_check_timeout( struct asrt_controller* c, uint32_t now )
{
        if ( now >= c->deadline ) {
                c->state = ASRT_CNTR_IDLE;
                return 1;
        }
        return 0;
}

//---------------------------------------------------------------------
// init

enum asrt_status asrt_cntr_start(
    struct asrt_controller* c,
    asrt_init_callback      cb,
    void*                   ptr,
    uint32_t                timeout )
{
        if ( !c || !cb )
                return ASRT_INIT_ERR;
        if ( !asrt_cntr_idle( c ) )
                return ASRT_BUSY_ERR;
        c->hndl.init = ( struct asrt_init_handler ){ .cb = cb, .ptr = ptr, .timeout = timeout };
        c->state     = ASRT_CNTR_INIT;
        c->stage     = ASRT_STAGE_INIT;
        return ASRT_SUCCESS;
}

void asrt_cntr_deinit( struct asrt_controller* c )
{
        ASRT_ASSERT( c );
        asrt_node_unlink( &c->node );
        if ( c->state == ASRT_CNTR_HNDL_DESC && c->hndl.desc.desc )
                asrt_free( &c->alloc, (void**) &c->hndl.desc.desc );
        else if ( c->state == ASRT_CNTR_HNDL_TI && c->hndl.ti.desc )
                asrt_free( &c->alloc, (void**) &c->hndl.ti.desc );
}

static enum asrt_status asrt_cntr_tick_init( struct asrt_controller* c, uint32_t now )
{
        ASRT_ASSERT( c->state == ASRT_CNTR_INIT );
        struct asrt_init_handler* h = &c->hndl.init;
        switch ( c->stage ) {
        case ASRT_STAGE_INIT:
                asrt_send_enque( &c->node, asrt_msg_ctor_proto_version( &h->msg ), NULL, NULL );
                c->stage    = ASRT_STAGE_WAITING;
                c->deadline = now + h->timeout;
                break;
        case ASRT_STAGE_WAITING:
                if ( asrt_cntr_check_timeout( c, now ) )
                        return h->cb( h->ptr, ASRT_TIMEOUT_ERR );
                break;
        case ASRT_STAGE_END: {
                enum asrt_status s;
                if ( h->ver.major != ASRT_PROTO_MAJOR ) {
                        ASRT_ERR_LOG(
                            "asrtc",
                            "Protocol version mismatch: got %u.%u.%u, expected %u.x.x",
                            h->ver.major,
                            h->ver.minor,
                            h->ver.patch,
                            ASRT_PROTO_MAJOR );
                        s = ASRT_VERSION_ERR;
                } else {
                        ASRT_INF_LOG( "asrtc", "Controller initialized" );
                        s = ASRT_SUCCESS;
                }
                c->state = ASRT_CNTR_IDLE;
                return h->cb( h->ptr, s );
        }
        }
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_cntr_recv_init(
    struct asrt_controller* c,
    enum asrt_message_id_e  eid,
    struct asrt_span*       buff )
{
        ASRT_ASSERT( c->state == ASRT_CNTR_INIT );

        if ( eid != ASRT_MSG_PROTO_VERSION )
                return ASRT_RECV_UNEXPECTED_ERR;
        if ( asrt_span_unfit_for( buff, 3 * sizeof( uint16_t ) ) )
                return ASRT_RECV_ERR;

        struct asrt_init_handler* h = &c->hndl.init;
        if ( c->stage != ASRT_STAGE_WAITING )  // XXX: can this get stuck?  // C02
                return ASRT_RECV_INTERNAL_ERR;

        asrt_cut_u16( &buff->b, &h->ver.major );
        asrt_cut_u16( &buff->b, &h->ver.minor );
        asrt_cut_u16( &buff->b, &h->ver.patch );

        c->stage = ASRT_STAGE_END;
        return ASRT_SUCCESS;
}

//---------------------------------------------------------------------
// test count

enum asrt_status asrt_cntr_test_count(
    struct asrt_controller*  c,
    asrt_test_count_callback cb,
    void*                    ptr,
    uint32_t                 timeout )
{
        ASRT_ASSERT( c && cb );
        if ( !asrt_cntr_idle( c ) )
                return ASRT_BUSY_ERR;

        c->hndl.tc = ( struct asrt_tc_handler ){
            .count   = 0,
            .ptr     = ptr,
            .cb      = cb,
            .timeout = timeout,
        };
        c->stage = ASRT_STAGE_INIT;
        c->state = ASRT_CNTR_HNDL_TC;
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_cntr_tick_test_count( struct asrt_controller* c, uint32_t now )
{
        ASRT_ASSERT( c->state == ASRT_CNTR_HNDL_TC );
        struct asrt_tc_handler* h = &c->hndl.tc;
        switch ( c->stage ) {
        case ASRT_STAGE_INIT:
                asrt_send_enque( &c->node, asrt_msg_ctor_test_count( &h->msg ), NULL, NULL );
                c->stage    = ASRT_STAGE_WAITING;
                c->deadline = now + h->timeout;
                break;
        case ASRT_STAGE_WAITING:
                if ( asrt_cntr_check_timeout( c, now ) )
                        return h->cb( h->ptr, ASRT_TIMEOUT_ERR, 0 );
                break;
        case ASRT_STAGE_END:
                h->cb( h->ptr, ASRT_SUCCESS, h->count );  // XXX: ignored return status - check
                                                          // everywhere here
                c->state = ASRT_CNTR_IDLE;
        }
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_cntr_recv_test_count(
    struct asrt_controller* c,
    enum asrt_message_id_e  eid,
    struct asrt_span*       buff )
{
        ASRT_ASSERT( c->state == ASRT_CNTR_HNDL_TC );

        if ( eid != ASRT_MSG_TEST_COUNT )
                return ASRT_RECV_UNEXPECTED_ERR;
        if ( asrt_span_unfit_for( buff, sizeof( uint16_t ) ) )
                return ASRT_RECV_ERR;
        struct asrt_tc_handler* h = &c->hndl.tc;
        if ( c->stage != ASRT_STAGE_WAITING )
                return ASRT_RECV_INTERNAL_ERR;

        asrt_cut_u16( &buff->b, &h->count );

        c->stage = ASRT_STAGE_END;
        return ASRT_SUCCESS;
}

//---------------------------------------------------------------------
// desc

enum asrt_status asrt_cntr_desc(
    struct asrt_controller* c,
    asrt_desc_callback      cb,
    void*                   ptr,
    uint32_t                timeout )
{
        ASRT_ASSERT( c && cb );

        if ( !asrt_cntr_idle( c ) )
                return ASRT_BUSY_ERR;

        c->hndl.desc = ( struct asrt_desc_handler ){
            .desc    = NULL,
            .ptr     = ptr,
            .cb      = cb,
            .timeout = timeout,
        };
        c->stage = ASRT_STAGE_INIT;
        c->state = ASRT_CNTR_HNDL_DESC;
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_cntr_tick_desc( struct asrt_controller* c, uint32_t now )
{
        ASRT_ASSERT( c->state == ASRT_CNTR_HNDL_DESC );
        struct asrt_desc_handler* h = &c->hndl.desc;
        switch ( c->stage ) {
        case ASRT_STAGE_INIT:
                asrt_send_enque( &c->node, asrt_msg_ctor_desc( &h->msg ), NULL, NULL );
                c->stage    = ASRT_STAGE_WAITING;
                c->deadline = now + h->timeout;
                break;
        case ASRT_STAGE_WAITING:
                if ( asrt_cntr_check_timeout( c, now ) )
                        return h->cb( h->ptr, ASRT_TIMEOUT_ERR, NULL );
                break;
        case ASRT_STAGE_END: {
                enum asrt_status res = h->cb( h->ptr, ASRT_SUCCESS, h->desc );
                asrt_free( &c->alloc, (void**) &h->desc );
                c->state = ASRT_CNTR_IDLE;
                return res;
        }
        }
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_cntr_recv_desc(
    struct asrt_controller* c,
    enum asrt_message_id_e  e,
    struct asrt_span*       buff )
{
        ASRT_ASSERT( c->state == ASRT_CNTR_HNDL_DESC );
        if ( e != ASRT_MSG_DESC )
                return ASRT_RECV_UNEXPECTED_ERR;

        struct asrt_desc_handler* h = &c->hndl.desc;
        if ( c->stage != ASRT_STAGE_WAITING )
                return ASRT_RECV_INTERNAL_ERR;

        h->desc = asrt_realloc_str( &c->alloc, buff );
        if ( h->desc == NULL )
                return ASRT_ALLOC_ERR;
        c->stage = ASRT_STAGE_END;

        return ASRT_SUCCESS;
}

//---------------------------------------------------------------------
// test info

enum asrt_status asrt_cntr_test_info(
    struct asrt_controller* c,
    uint16_t                id,
    asrt_test_info_callback cb,
    void*                   ptr,
    uint32_t                timeout )
{
        ASRT_ASSERT( c && cb );
        if ( !asrt_cntr_idle( c ) )
                return ASRT_BUSY_ERR;

        c->hndl.ti = ( struct asrt_ti_handler ){
            .tid     = id,
            .result  = ASRT_TEST_INFO_SUCCESS,
            .desc    = NULL,
            .ptr     = ptr,
            .cb      = cb,
            .timeout = timeout,
        };
        c->stage = ASRT_STAGE_INIT;
        c->state = ASRT_CNTR_HNDL_TI;
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_cntr_tick_test_info( struct asrt_controller* c, uint32_t now )
{
        ASRT_ASSERT( c->state == ASRT_CNTR_HNDL_TI );
        struct asrt_ti_handler* h = &c->hndl.ti;
        switch ( c->stage ) {
        case ASRT_STAGE_INIT:
                asrt_send_enque( &c->node, asrt_msg_ctor_test_info( &h->msg, h->tid ), NULL, NULL );
                c->stage    = ASRT_STAGE_WAITING;
                c->deadline = now + h->timeout;
                break;
        case ASRT_STAGE_WAITING:
                if ( asrt_cntr_check_timeout( c, now ) )
                        return h->cb( h->ptr, ASRT_TIMEOUT_ERR, h->tid, NULL );
                break;
        case ASRT_STAGE_END: {
                enum asrt_status cb_status =
                    h->result == ASRT_TEST_INFO_SUCCESS ? ASRT_SUCCESS : ASRT_RECV_UNEXPECTED_ERR;
                enum asrt_status res = h->cb( h->ptr, cb_status, h->tid, h->desc );
                asrt_free( &c->alloc, (void**) &h->desc );
                c->state = ASRT_CNTR_IDLE;
                return res;
        }
        }
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_cntr_recv_test_info(
    struct asrt_controller* c,
    enum asrt_message_id_e  eid,
    struct asrt_span*       buff )
{
        ASRT_ASSERT( c->state == ASRT_CNTR_HNDL_TI );
        if ( eid != ASRT_MSG_TEST_INFO )
                return ASRT_RECV_UNEXPECTED_ERR;

        if ( asrt_span_unfit_for( buff, sizeof( uint16_t ) + sizeof( uint8_t ) ) )
                return ASRT_RECV_ERR;

        struct asrt_ti_handler* h = &c->hndl.ti;
        if ( c->stage != ASRT_STAGE_WAITING )
                return ASRT_RECV_INTERNAL_ERR;
        uint16_t tid;
        asrt_cut_u16( &buff->b, &tid );
        if ( tid != h->tid ) {
                ASRT_ERR_LOG(
                    "asrtc", "Test info response tid mismatch: got %u, expected %u", tid, h->tid );
                return ASRT_RECV_UNEXPECTED_ERR;
        }

        uint8_t res = *buff->b++;
        if ( res != ASRT_TEST_INFO_SUCCESS && res != ASRT_TEST_INFO_MISSING_TEST_ERR ) {
                ASRT_ERR_LOG( "asrtc", "Invalid test info result code: %u", res );
                return ASRT_RECV_UNEXPECTED_ERR;
        }
        h->result = (asrt_test_info_result) res;

        h->desc = asrt_realloc_str( &c->alloc, buff );
        if ( h->desc == NULL )
                return ASRT_ALLOC_ERR;
        c->stage = ASRT_STAGE_END;

        return ASRT_SUCCESS;
}

//---------------------------------------------------------------------
// test info

enum asrt_status asrt_cntr_test_exec(
    struct asrt_controller*   c,
    uint16_t                  id,
    asrt_test_result_callback cb,
    void*                     ptr,
    uint32_t                  timeout )
{
        ASRT_ASSERT( c && cb );
        if ( !asrt_cntr_idle( c ) )
                return ASRT_BUSY_ERR;
        c->hndl.exec = ( struct asrt_exec_handler ){
            .res =
                ( struct asrt_result ){
                    .test_id = id,
                    .run_id  = c->run_id++,
                    .res     = ASRT_TEST_RESULT_ERROR,
                },
            .ptr     = ptr,
            .cb      = cb,
            .timeout = timeout,
        };
        c->stage = ASRT_STAGE_INIT;
        c->state = ASRT_CNTR_HNDL_EXEC;
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_cntr_tick_test_exec( struct asrt_controller* c, uint32_t now )
{
        struct asrt_exec_handler* h = &c->hndl.exec;
        switch ( c->stage ) {
        case ASRT_STAGE_INIT:
                asrt_send_enque(
                    &c->node,
                    asrt_msg_ctor_test_start( &h->msg, h->res.test_id, h->res.run_id ),
                    NULL,
                    NULL );
                c->stage    = ASRT_STAGE_WAITING;
                c->deadline = now + h->timeout;
                break;
        case ASRT_STAGE_WAITING:
                if ( asrt_cntr_check_timeout( c, now ) )
                        return h->cb( h->ptr, ASRT_TIMEOUT_ERR, &h->res );
                break;
        case ASRT_STAGE_END: {
                enum asrt_status res = h->cb( h->ptr, ASRT_SUCCESS, &h->res );
                c->state             = ASRT_CNTR_IDLE;
                return res;
        }
        }
        return ASRT_SUCCESS;
}

static enum asrt_status asrt_cntr_recv_test_exec(
    struct asrt_controller* c,
    enum asrt_message_id_e  eid,
    struct asrt_span*       buff )
{
        ASRT_ASSERT( c->state == ASRT_CNTR_HNDL_EXEC );
        struct asrt_exec_handler* h = &c->hndl.exec;
        if ( c->stage != ASRT_STAGE_WAITING )
                return ASRT_RECV_INTERNAL_ERR;
        switch ( eid ) {
        case ASRT_MSG_TEST_START: {
                if ( asrt_span_unfit_for( buff, sizeof( uint16_t ) + sizeof( uint32_t ) ) )
                        return ASRT_RECV_ERR;
                // XXX should not be ignored
                uint16_t* tid = &h->res.test_id;
                asrt_cut_u16( &buff->b, tid );
                // XXX: should not be ignored
                uint32_t* rid = &h->res.run_id;
                asrt_cut_u32( &buff->b, rid );
                break;
        }
        case ASRT_MSG_TEST_RESULT: {
                if ( asrt_span_unfit_for( buff, sizeof( uint32_t ) + sizeof( asrt_test_result ) ) )
                        return ASRT_RECV_ERR;
                uint32_t rid;
                // XXX: should not be ignored
                asrt_cut_u32( &buff->b, &rid );
                uint16_t res;
                // XXX: should not be ignored
                asrt_cut_u16( &buff->b, &res );

                if ( rid != h->res.run_id ) {
                        ASRT_ERR_LOG(
                            "asrtc_main",
                            "Received test result for unexpected run: %u (expected %u)",
                            rid,
                            h->res.run_id );
                        h->res.res = ASRT_TEST_RESULT_ERROR;
                } else {
                        ASRT_INF_LOG( "asrtc_main", "Received test result: %u", res );
                        h->res.res = res;
                }

                c->stage = ASRT_STAGE_END;
                break;
        }
        default:
                return ASRT_RECV_UNEXPECTED_ERR;
        }

        return ASRT_SUCCESS;
}

//---------------------------------------------------------------------
// tick

static enum asrt_status asrt_cntr_tick( struct asrt_controller* c, uint32_t now )
{
        switch ( c->state ) {
        case ASRT_CNTR_INIT:
                return asrt_cntr_tick_init( c, now );
        case ASRT_CNTR_HNDL_TC:
                return asrt_cntr_tick_test_count( c, now );
        case ASRT_CNTR_HNDL_DESC:
                return asrt_cntr_tick_desc( c, now );
        case ASRT_CNTR_HNDL_TI:
                return asrt_cntr_tick_test_info( c, now );
        case ASRT_CNTR_HNDL_EXEC:
                return asrt_cntr_tick_test_exec( c, now );
        case ASRT_CNTR_IDLE:
                break;
        }
        return ASRT_SUCCESS;
}

uint32_t asrt_cntr_idle( struct asrt_controller const* c )
{
        return c->state == ASRT_CNTR_IDLE;
}

static enum asrt_status asrt_cntr_recv( void* data, struct asrt_span buff )
{
        ASRT_ASSERT( data );
        struct asrt_controller* c = (struct asrt_controller*) data;
        asrt_message_id         id;
        if ( asrt_span_unfit_for( &buff, sizeof( asrt_message_id ) ) )
                return ASRT_RECV_ERR;
        asrt_cut_u16( &buff.b, &id );

        enum asrt_message_id_e eid = (enum asrt_message_id_e) id;
        enum asrt_status       st  = ASRT_SUCCESS;
        switch ( c->state ) {
        case ASRT_CNTR_INIT:
                st = asrt_cntr_recv_init( c, eid, &buff );
                break;
        case ASRT_CNTR_HNDL_TC:
                st = asrt_cntr_recv_test_count( c, eid, &buff );
                break;
        case ASRT_CNTR_HNDL_DESC:
                st = asrt_cntr_recv_desc( c, eid, &buff );
                break;
        case ASRT_CNTR_HNDL_TI:
                st = asrt_cntr_recv_test_info( c, eid, &buff );
                break;
        case ASRT_CNTR_HNDL_EXEC:
                st = asrt_cntr_recv_test_exec( c, eid, &buff );
                break;
        case ASRT_CNTR_IDLE:
                return ASRT_RECV_UNEXPECTED_ERR;
        }
        if ( st != ASRT_SUCCESS )
                return st;
        return buff.b == buff.e ? ASRT_SUCCESS : ASRT_RECV_TRAILING_ERR;
}

static enum asrt_status asrt_cntr_event( void* p, enum asrt_event_e e, void* arg )
{
        struct asrt_controller* c = (struct asrt_controller*) p;
        switch ( e ) {
        case ASRT_EVENT_TICK:
                return asrt_cntr_tick( c, *(uint32_t*) arg );
        case ASRT_EVENT_RECV:
                return asrt_cntr_recv( c, *(struct asrt_span*) arg );
        }
        ASRT_ERR_LOG( "asrtc", "unexpected event: %s", asrt_event_to_str( e ) );
        return ASRT_INVALID_EVENT_ERR;
}

enum asrt_status asrt_cntr_init(
    struct asrt_controller*    c,
    struct asrt_send_req_list* send_queue,
    struct asrt_allocator      alloc )
{
        if ( !c )
                return ASRT_INIT_ERR;
        *c = ( struct asrt_controller ){
            .node =
                ( struct asrt_node ){
                    .chid       = ASRT_CORE,
                    .e_cb_ptr   = c,
                    .e_cb       = &asrt_cntr_event,
                    .next       = NULL,
                    .prev       = NULL,
                    .send_queue = send_queue,
                },
            .alloc    = alloc,
            .run_id   = 0,
            .state    = ASRT_CNTR_IDLE,
            .stage    = ASRT_STAGE_INIT,
            .deadline = 0,
        };

        return ASRT_SUCCESS;
}
