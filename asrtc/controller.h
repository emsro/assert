
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

#ifdef __cplusplus
extern "C" {
#endif

#include "../asrtl/allocator.h"
#include "../asrtl/chann.h"
#include "../asrtl/span.h"
#include "../asrtl/status.h"
#include "./handlers.h"

enum asrt_cntr_state
{
        ASRT_CNTR_INIT      = 0x01,
        ASRT_CNTR_IDLE      = 0x02,
        ASRT_CNTR_HNDL_TC   = 0x10,
        ASRT_CNTR_HNDL_DESC = 0x11,
        ASRT_CNTR_HNDL_TI   = 0x12,
        ASRT_CNTR_HNDL_EXEC = 0x13,
};

struct asrt_controller
{
        struct asrt_node      node;
        struct asrt_allocator alloc;

        uint32_t             run_id;
        enum asrt_cntr_state state;
        enum asrt_stage_e    stage;
        uint32_t             deadline;
        union
        {
                struct asrt_init_handler init;
                struct asrt_tc_handler   tc;
                struct asrt_desc_handler desc;
                struct asrt_ti_handler   ti;
                struct asrt_exec_handler exec;
        } hndl;
};

enum asrt_status asrt_cntr_init(
    struct asrt_controller*    c,
    struct asrt_send_req_list* send_queue,
    struct asrt_allocator      alloc );

enum asrt_status asrt_cntr_start(
    struct asrt_controller* c,
    asrt_init_callback      cb,
    void*                   ptr,
    uint32_t                timeout );

uint32_t asrt_cntr_idle( struct asrt_controller const* c );

enum asrt_status asrt_cntr_desc(
    struct asrt_controller* c,
    asrt_desc_callback      cb,
    void*                   ptr,
    uint32_t                timeout );
enum asrt_status asrt_cntr_test_count(
    struct asrt_controller*  c,
    asrt_test_count_callback cb,
    void*                    ptr,
    uint32_t                 timeout );
enum asrt_status asrt_cntr_test_info(
    struct asrt_controller* c,
    uint16_t                id,
    asrt_test_info_callback cb,
    void*                   ptr,
    uint32_t                timeout );

enum asrt_status asrt_cntr_test_exec(
    struct asrt_controller*   c,
    uint16_t                  id,
    asrt_test_result_callback cb,
    void*                     ptr,
    uint32_t                  timeout );

void asrt_cntr_deinit( struct asrt_controller* c );

#ifdef __cplusplus
}
#endif

#endif
