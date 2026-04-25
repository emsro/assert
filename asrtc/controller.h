
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
        struct asrt_node      node;
        struct asrt_sender    sendr;
        struct asrt_allocator alloc;

        uint32_t              run_id;
        enum asrtc_cntr_state state;
        enum asrtc_stage_e    stage;
        uint32_t              deadline;
        union
        {
                struct asrtc_init_handler init;
                struct asrtc_tc_handler   tc;
                struct asrtc_desc_handler desc;
                struct asrtc_ti_handler   ti;
                struct asrtc_exec_handler exec;
        } hndl;
};

enum asrt_status asrtc_cntr_init(
    struct asrtc_controller* c,
    struct asrt_sender       s,
    struct asrt_allocator    alloc );

enum asrt_status asrtc_cntr_start(
    struct asrtc_controller* c,
    asrtc_init_callback      cb,
    void*                    ptr,
    uint32_t                 timeout );

uint32_t asrtc_cntr_idle( struct asrtc_controller const* c );

enum asrt_status asrtc_cntr_desc(
    struct asrtc_controller* c,
    asrtc_desc_callback      cb,
    void*                    ptr,
    uint32_t                 timeout );
enum asrt_status asrtc_cntr_test_count(
    struct asrtc_controller*  c,
    asrtc_test_count_callback cb,
    void*                     ptr,
    uint32_t                  timeout );
enum asrt_status asrtc_cntr_test_info(
    struct asrtc_controller* c,
    uint16_t                 id,
    asrtc_test_info_callback cb,
    void*                    ptr,
    uint32_t                 timeout );

enum asrt_status asrtc_cntr_test_exec(
    struct asrtc_controller*   c,
    uint16_t                   id,
    asrtc_test_result_callback cb,
    void*                      ptr,
    uint32_t                   timeout );

void asrtc_cntr_deinit( struct asrtc_controller* c );

#ifdef __cplusplus
}
#endif

#endif
