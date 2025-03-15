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

#include "../asrtl/core_proto.h"

#include <string.h>

enum asrtc_status asrtc_cntr_init(
    struct asrtc_controller* c,
    struct asrtl_sender      s,
    struct asrtc_allocator   alloc )
{
        if ( !c )
                return ASRTC_CNTR_INIT_ERR;
        *c = ( struct asrtc_controller ){
            .node =
                ( struct asrtl_node ){
                    .chid     = ASRTL_CORE,
                    .recv_ptr = c,
                    .recv_cb  = &asrtc_cntr_recv,
                    .next     = NULL,
                },
            .sendr     = s,
            .alloc     = alloc,
            .state     = ASRTC_CNTR_INIT,
            .hndl.init = ( struct asrtc_init_handler ){ .stage = 0 },
        };

        return ASRTC_SUCCESS;
}

static inline enum asrtl_status asrtc_send( struct asrtc_controller* c, uint8_t* b, uint8_t* e )
{
        assert( c && b && e );
        return asrtl_send( &c->sendr, ASRTL_CORE, ( struct asrtl_span ){ b, e } );
}

enum asrtc_status asrtc_cntr_tick( struct asrtc_controller* c )
{
        uint8_t           buffer[32];
        struct asrtl_span buff = ( struct asrtl_span ){ .b = buffer, .e = buffer + sizeof buffer };
        struct asrtl_span sp   = buff;
        switch ( c->state ) {
        case ASRTC_CNTR_INIT: {
                struct asrtc_init_handler* h = &c->hndl.init;
                if ( h->stage == 0 ) {
                        if ( asrtl_msg_ctor_proto_version( &sp ) != ASRTL_SUCCESS )
                                return ASRTC_SEND_ERR;
                        if ( asrtc_send( c, buff.b, sp.b ) != ASRTL_SUCCESS )
                                return ASRTC_SEND_ERR;
                        h->stage += 1;
                } else if ( h->stage == 1 ) {
                        break;
                } else {
                        // XXX: check the version
                        c->state = ASRTC_CNTR_IDLE;
                }
                break;
        }
        case ASRTC_CNTR_HNDL_TC: {
                struct asrtc_tc_handler* h = &c->hndl.tc;
                if ( h->stage == 0 ) {
                        if ( asrtl_msg_ctor_test_count( &sp ) != ASRTL_SUCCESS )
                                return ASRTC_SEND_ERR;
                        if ( asrtc_send( c, buff.b, sp.b ) != ASRTL_SUCCESS )
                                return ASRTC_SEND_ERR;
                        h->stage = 1;
                } else if ( h->stage == 1 ) {
                        break;
                } else {
                        h->cb( h->ptr, h->count );
                        c->state = ASRTC_CNTR_IDLE;
                }
                break;
        }
        case ASRTC_CNTR_HNDL_DESC: {
                struct asrtc_desc_handler* h = &c->hndl.desc;
                if ( h->stage == 0 ) {
                        if ( asrtl_msg_ctor_desc( &sp ) != ASRTL_SUCCESS )
                                return ASRTC_SEND_ERR;
                        if ( asrtc_send( c, buff.b, sp.b ) != ASRTL_SUCCESS )
                                return ASRTC_SEND_ERR;
                        h->stage = 1;
                } else if ( h->stage == 1 ) {
                        break;
                } else {
                        h->cb( h->ptr, h->desc );
                        asrtc_free( &c->alloc, (void**) &h->desc );
                        c->state = ASRTC_CNTR_IDLE;
                }
                break;
        }
        case ASRTC_CNTR_HNDL_TI: {
                struct asrtc_ti_handler* h = &c->hndl.ti;
                if ( h->stage == 0 ) {
                        if ( asrtl_msg_ctor_test_info( &sp, h->tid ) != ASRTL_SUCCESS )
                                return ASRTC_SEND_ERR;
                        if ( asrtc_send( c, buff.b, sp.b ) != ASRTL_SUCCESS )
                                return ASRTC_SEND_ERR;
                        h->stage = 1;
                } else if ( h->stage == 1 ) {
                        break;
                } else {
                        h->cb( h->ptr, h->desc );
                        asrtc_free( &c->alloc, (void**) &h->desc );
                        c->state = ASRTC_CNTR_IDLE;
                }
                break;
        }
        case ASRTC_CNTR_HNDL_EXEC: {
                struct asrtc_exec_handler* h = &c->hndl.exec;
                if ( h->stage == 0 ) {
                        if ( asrtl_msg_ctor_test_start( &sp, h->res.test_id ) != ASRTL_SUCCESS )
                                return ASRTC_SEND_ERR;
                        if ( asrtc_send( c, buff.b, sp.b ) != ASRTL_SUCCESS )
                                return ASRTC_SEND_ERR;
                        h->stage = 1;
                } else if ( h->stage == 1 ) {
                        break;
                } else {
                        h->cb( h->ptr, &h->res );
                        c->state = ASRTC_CNTR_IDLE;
                }
                break;
        }
        case ASRTC_CNTR_IDLE: {
                break;
        }
        }
        return ASRTC_SUCCESS;
}

uint32_t asrtc_cntr_idle( struct asrtc_controller* c )
{
        return c->state == ASRTC_CNTR_IDLE;
}

enum asrtc_status asrtc_cntr_desc(
    struct asrtc_controller* c,
    asrtc_test_desc_callback cb,
    void*                    ptr )
{
        assert( c && cb );

        if ( !asrtc_cntr_idle( c ) )
                return ASRTC_CNTR_BUSY_ERR;

        c->hndl.desc = ( struct asrtc_desc_handler ){
            .stage = 0,
            .desc  = NULL,
            .ptr   = ptr,
            .cb    = cb,
        };
        c->state = ASRTC_CNTR_HNDL_DESC;
        return ASRTC_SUCCESS;
}

enum asrtc_status asrtc_cntr_test_count(
    struct asrtc_controller*  c,
    asrtc_test_count_callback cb,
    void*                     ptr )
{
        assert( c && cb );
        if ( !asrtc_cntr_idle( c ) )
                return ASRTC_CNTR_BUSY_ERR;

        c->hndl.tc = ( struct asrtc_tc_handler ){
            .stage = 0,
            .count = 0,
            .ptr   = ptr,
            .cb    = cb,
        };
        c->state = ASRTC_CNTR_HNDL_TC;
        return ASRTC_SUCCESS;
}
enum asrtc_status asrtc_cntr_test_info(
    struct asrtc_controller* c,
    uint16_t                 id,
    asrtc_test_info_callback cb,
    void*                    ptr )
{
        assert( c && cb );
        if ( !asrtc_cntr_idle( c ) )
                return ASRTC_CNTR_BUSY_ERR;

        c->hndl.ti = ( struct asrtc_ti_handler ){
            .tid   = id,
            .stage = 0,
            .desc  = NULL,
            .ptr   = ptr,
            .cb    = cb,
        };
        c->state = ASRTC_CNTR_HNDL_TI;
        return ASRTC_SUCCESS;
}

enum asrtl_status asrtc_cntr_recv( void* data, struct asrtl_span buff )
{
        assert( data );
        struct asrtc_controller* c = (struct asrtc_controller*) data;
        asrtl_message_id         id;
        if ( asrtl_buffer_unfit( &buff, sizeof( asrtl_message_id ) ) )
                return ASRTL_RECV_ERR;
        asrtl_cut_u16( &buff.b, &id );

        enum asrtl_message_id_e eid = (enum asrtl_message_id_e) id;
        switch ( eid ) {
        case ASRTL_MSG_PROTO_VERSION: {
                if ( asrtl_buffer_unfit( &buff, 3 * sizeof( uint16_t ) ) )
                        return ASRTL_RECV_ERR;
                if ( c->state != ASRTC_CNTR_INIT )
                        return ASRTL_RECV_UNEXPECTED_ERR;

                struct asrtc_init_handler* h = &c->hndl.init;
                if ( h->stage != 1 )
                        return ASRTL_RECV_INTERNAL_ERR;

                asrtl_cut_u16( &buff.b, &h->ver.major );
                asrtl_cut_u16( &buff.b, &h->ver.minor );
                asrtl_cut_u16( &buff.b, &h->ver.patch );

                h->stage += 1;
                break;
        }
        case ASRTL_MSG_DESC: {
                if ( c->state != ASRTC_CNTR_HNDL_DESC )
                        return ASRTL_RECV_UNEXPECTED_ERR;

                struct asrtc_desc_handler* h = &c->hndl.desc;
                if ( h->stage != 1 )
                        return ASRTL_RECV_INTERNAL_ERR;

                h->desc = asrtc_realloc_str( &c->alloc, &buff );
                if ( h->desc == NULL )
                        return ASRTL_RECV_ERR;
                h->stage = 2;
                break;
        }
        case ASRTL_MSG_TEST_COUNT: {
                if ( asrtl_buffer_unfit( &buff, sizeof( uint16_t ) ) )
                        return ASRTL_RECV_ERR;
                if ( c->state != ASRTC_CNTR_HNDL_TC )
                        return ASRTL_RECV_UNEXPECTED_ERR;
                struct asrtc_tc_handler* h = &c->hndl.tc;
                if ( h->stage != 1 )
                        return ASRTL_RECV_INTERNAL_ERR;

                asrtl_cut_u16( &buff.b, &h->count );

                h->stage += 1;
                break;
        }
        case ASRTL_MSG_TEST_INFO: {
                if ( asrtl_buffer_unfit( &buff, sizeof( uint16_t ) ) )
                        return ASRTL_RECV_ERR;
                if ( c->state != ASRTC_CNTR_HNDL_TI )
                        return ASRTL_RECV_UNEXPECTED_ERR;

                struct asrtc_ti_handler* h = &c->hndl.ti;
                if ( h->stage != 1 )
                        return ASRTL_RECV_INTERNAL_ERR;
                uint16_t tid;  // XXX: unused for now
                asrtl_cut_u16( &buff.b, &tid );

                h->desc = asrtc_realloc_str( &c->alloc, &buff );
                if ( h->desc == NULL )
                        return ASRTL_RECV_ERR;
                h->stage = 2;
                break;
        }
        case ASRTL_MSG_TEST_START:
        case ASRTL_MSG_ERROR:
        case ASRTL_MSG_TEST_RESULT:
        default:
                return ASRTL_RECV_UNKNOWN_ID_ERR;
        }

        return buff.b == buff.e ? ASRTL_SUCCESS : ASRTL_RECV_ERR;
}
