
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

/// Internal state machine states for the controller's CORE channel.
enum asrt_cntr_state
{
        ASRT_CNTR_INIT      = 0x01,  ///< Freshly initialised, not yet started.
        ASRT_CNTR_IDLE      = 0x02,  ///< No operation outstanding.
        ASRT_CNTR_HNDL_TC   = 0x10,  ///< Waiting for TEST_COUNT reply.
        ASRT_CNTR_HNDL_DESC = 0x11,  ///< Waiting for DESC reply.
        ASRT_CNTR_HNDL_TI   = 0x12,  ///< Waiting for TEST_INFO reply.
        ASRT_CNTR_HNDL_EXEC = 0x13,  ///< Waiting for TEST_RESULT.
};

/// Controller module — CORE channel, host side.
///
/// Drives the test session: protocol handshake, test enumeration, and
/// one-at-a-time test execution.  All operations are non-blocking; the
/// caller advances the state machine by ticking the channel periodically.
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

/// Initialise the controller and link it into @p send_queue.
/// Must be called before any other asrt_cntr_* function.
enum asrt_status asrt_cntr_init(
    struct asrt_controller*    c,
    struct asrt_send_req_list* send_queue,
    struct asrt_allocator      alloc );

/// Begin the protocol handshake.  Sends PROTO_VERSION and waits for the reply.
/// @p cb is invoked (with @p ptr) when the handshake completes or times out.
enum asrt_status asrt_cntr_start(
    struct asrt_controller* c,
    asrt_init_callback      cb,
    void*                   ptr,
    uint32_t                timeout );

/// Returns non-zero if the controller is idle (no operation outstanding).
uint32_t asrt_cntr_idle( struct asrt_controller const* c );

/// Query the target description string.  @p cb receives the C-string on success.
enum asrt_status asrt_cntr_desc(
    struct asrt_controller* c,
    asrt_desc_callback      cb,
    void*                   ptr,
    uint32_t                timeout );
/// Query the number of registered tests.  @p cb receives the count.
enum asrt_status asrt_cntr_test_count(
    struct asrt_controller*  c,
    asrt_test_count_callback cb,
    void*                    ptr,
    uint32_t                 timeout );
/// Query the name/description of test @p id.  @p cb receives the C-string.
enum asrt_status asrt_cntr_test_info(
    struct asrt_controller* c,
    uint16_t                id,
    asrt_test_info_callback cb,
    void*                   ptr,
    uint32_t                timeout );

/// Execute test @p id.  @p cb is invoked with the pass/fail/error result.
enum asrt_status asrt_cntr_test_exec(
    struct asrt_controller*   c,
    uint16_t                  id,
    asrt_test_result_callback cb,
    void*                     ptr,
    uint32_t                  timeout );

/// Release any resources owned by the controller.
void asrt_cntr_deinit( struct asrt_controller* c );

#ifdef __cplusplus
}
#endif

#endif
