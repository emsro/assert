
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
#ifndef ASRTC_CONTROLLER_H
#define ASRTC_CONTROLLER_H

#include "../asrtl/chann.h"
#include "../asrtl/util.h"
#include "./allocator.h"
#include "./handlers.h"
#include "./status.h"

enum asrtc_cntr_state
{
        ASRTC_CNTR_INIT      = 0x01,
        ASRTC_CNTR_IDLE      = 0x02,
        ASRTC_CNTR_HNDL_TC   = 0x10,
        ASRTC_CNTR_HNDL_DESC = 0x11,
        ASRTC_CNTR_HNDL_TI   = 0x12,
        ASRTC_CNTR_HNDL_EXEC = 0x13,
};

struct asrtc_controller
{
        struct asrtl_node      node;
        struct asrtl_sender    sendr;
        struct asrtc_allocator alloc;

        enum asrtc_cntr_state state;
        union
        {
                struct asrtc_init_handler init;
                struct asrtc_tc_handler   tc;
                struct asrtc_desc_handler desc;
                struct asrtc_ti_handler   ti;
                struct asrtc_exec_handler exec;
        } hndl;
};

enum asrtc_status asrtc_cntr_init(
    struct asrtc_controller* c,
    struct asrtl_sender      s,
    struct asrtc_allocator   alloc );

enum asrtc_status asrtc_cntr_tick( struct asrtc_controller* c );
uint32_t          asrtc_cntr_idle( struct asrtc_controller* c );

enum asrtc_status asrtc_cntr_desc(
    struct asrtc_controller* c,
    asrtc_test_desc_callback cb,
    void*                    ptr );
enum asrtc_status asrtc_cntr_test_count(
    struct asrtc_controller*  c,
    asrtc_test_count_callback cb,
    void*                     ptr );
enum asrtc_status asrtc_cntr_test_info(
    struct asrtc_controller* c,
    uint16_t                 id,
    asrtc_test_info_callback cb,
    void*                    ptr );

enum asrtc_status asrtc_cntr_test_exec(
    struct asrtc_controller*   c,
    uint16_t                   id,
    asrtc_test_result_callback cb,
    void*                      ptr );

enum asrtl_status asrtc_cntr_recv( void* data, struct asrtl_span buff );

#endif
